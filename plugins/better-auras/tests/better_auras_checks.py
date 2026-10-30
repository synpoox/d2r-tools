"""Better Auras named checks for d2rAI-toolkit's testing/ framework (game_checks.py).

Use: add this folder to [checks] paths and "better_auras_checks" to [checks] modules in modkit-testing.toml and set
[logs] plugin_log to the plugin's log (under d2rloader/logs/). Better Auras is game-wide: the checks look for the DLL in
<slot>/d2rloader/plugins/ and its config in <slot>/d2rloader/config/, not in the mod folder.

  python testing/game_checks.py run ba-smoke --slot N        unattended: files, load, every part on, no crash
  python testing/game_checks.py run ba-scenario --slot N     after a play session in that slot (TESTING.md): reads
                                                             the plugin log for each scenario's evidence

The scenario checks read the log because this plugin has no input harness yet: someone (or a future harness) plays
the TESTING.md steps; the checks then prove from the log what the plugin did. A scenario with no evidence is
skipped, not passed.
"""
import os
import re
import time

from game_checks import check
import game_ctl

try:
    import tomllib
except ImportError:  # Python < 3.11
    tomllib = None

PLUGIN = "better-auras"
PARTS = ("config", "toggle", "drain", "reserve", "decouple")


def _plugin_log(ctx) -> str:
    path = ctx.cfg.plugin_log(ctx.slot)
    if not path:
        ctx.skip("[logs] plugin_log is not set in the testing config")
    if not os.path.isfile(path):
        ctx.skip(f"no plugin log yet: {path}")
    with open(path, encoding="utf-8", errors="replace") as f:
        return f.read()


def _last_session(text: str) -> str:
    """The log from the last plugin load on."""
    i = text.rfind(f"{PLUGIN}: ")
    starts = [m.start() for m in re.finditer(rf"{PLUGIN}: \S+ loaded \(", text)]
    return text[starts[-1]:] if starts else text if i >= 0 else ""


@check("ba-files", tags=["ba-smoke", "offline"])
def ba_files(ctx):
    """better-auras.dll is in the game-wide plugins folder (and not in the mod's), and its config parses."""
    root = ctx.cfg.slot_dir(ctx.slot)
    dll = os.path.join(root, "d2rloader", "plugins", f"{PLUGIN}.dll")
    toml = os.path.join(root, "d2rloader", "config", f"{PLUGIN}.toml")
    assert os.path.isfile(dll), f"missing {dll}"
    stray = os.path.join(ctx.cfg.plugins_dir(ctx.slot), f"{PLUGIN}.dll")
    assert not os.path.isfile(stray), f"a second copy in the mod folder: {stray}"
    if not os.path.isfile(toml):
        ctx.skip(f"no {toml} yet: D2RLoader writes it on the plugin's first start")
    if tomllib is None:
        return "files present (no tomllib to parse the config)"
    with open(toml, "rb") as f:
        data = tomllib.load(f)
    skills = data.get("skills", {})
    assert skills, "the config lists no [skills.<name>]"
    for name, s in skills.items():
        for k in s:
            assert k in ("enabled", "toggle", "trigger", "drain", "reserve"), f"skills.{name}.{k}: unknown setting"
    return f"{len(skills)} skill(s) configured"


@check("ba-loads", tags=["ba-smoke", "game"])
def ba_loads(ctx):
    """The plugin loads and every part reports on (no refused hook, no config error)."""
    ctx.ensure_running()

    def loaded():
        try:
            text = _last_session(_plugin_log(ctx))
        except Exception:
            return False
        return all(re.search(rf"{PLUGIN}: {p} (on|off)", text) for p in PARTS)

    try:
        game_ctl.wait_for(loaded, 90, 2.0, "the plugin's part lines in its log")
    except game_ctl.WaitTimeout:
        raise AssertionError("the plugin log never listed every part (plugin not loaded?)")
    text = _last_session(_plugin_log(ctx))
    assert "MISMATCH" not in text, "image mismatch: every engine part is off"
    assert "REFUSED" not in text, "a hook was refused: " + re.search(r"hooks[^\n]*", text).group(0)
    off = [p for p in PARTS if re.search(rf"{PLUGIN}: {p} off", text)]
    assert not off, f"parts off: {off} (" + "; ".join(re.search(rf"{PLUGIN}: {p} off[^\n]*", text).group(0) for p in off) + ")"
    warns = re.findall(rf"{PLUGIN}: config line [^\n]*", text)
    return "all parts on" + (f"; config warnings: {warns}" if warns else "")


@check("ba-no-crash", tags=["ba-smoke"])
def ba_no_crash(ctx):
    """The loader wrote no crash file since the run began."""
    new = ctx.game.new_crash_files(ctx.shared.get("crash_before", []))
    assert not new, f"new crash files: {new}"
    return "no crash files"


# --- Scenario checks: evidence in the plugin log after the TESTING.md steps ------------------------------------------

def _scenario(ctx, pattern: str, what: str, forbid: str | None = None) -> str:
    text = _last_session(_plugin_log(ctx))
    hits = re.findall(pattern, text)
    if not hits:
        ctx.skip(f"no evidence of {what} in the last session (play the TESTING.md step first)")
    if forbid:
        bad = re.findall(forbid, text)
        assert not bad, f"unexpected: {bad[:3]}"
    return f"{len(hits)}x: {hits[-1] if isinstance(hits[-1], str) else hits[-1][0]}"[:160]


@check("ba-toggle", tags=["ba-scenario"])
def ba_toggle(ctx):
    """A listed aura went on and later off by its trigger."""
    text = _last_session(_plugin_log(ctx))
    on = re.findall(r"aura (\d+) on \(level", text)
    off = re.findall(r"aura (\d+) off \(toggled off\)", text)
    if not on:
        ctx.skip("no aura was toggled on")
    assert off, f"auras {sorted(set(on))} went on but none was toggled off"
    return f"on {len(on)}x, off {len(off)}x"


@check("ba-survives-switch", tags=["ba-scenario"])
def ba_survives_switch(ctx):
    """A toggled aura was kept through a right-skill switch (it was rebuilt, not dropped)."""
    return _scenario(ctx, r"aura \d+ kept through the skill switch", "a skill switch with a toggled aura on",
                     forbid=r"aura \d+ off \(could not be kept through the skill switch\)")


@check("ba-drain-stop", tags=["ba-scenario"])
def ba_drain_stop(ctx):
    """An aura with drain stopped when the pool could not pay."""
    return _scenario(ctx, r"aura \d+ off \(drain could not be paid\)", "a drain running dry")


@check("ba-reserve-refusal", tags=["ba-scenario"])
def ba_reserve_refusal(ctx):
    """An activation was refused because the pool could not cover the reservation."""
    return _scenario(ctx, r"aura \d+ refused: not enough to reserve", "a refused reservation")


@check("ba-reserve-apply", tags=["ba-scenario"])
def ba_reserve_apply(ctx):
    """A reservation was applied (amount in the on line). The capped orb is read on screen."""
    return _scenario(ctx, r"aura \d+ on \(level \d+, reserve [1-9]\d* (?:mana|life)\)", "an aura with a reservation")


@check("ba-death", tags=["ba-scenario"])
def ba_death(ctx):
    """Dying turned every aura off."""
    return _scenario(ctx, r"aura \d+ off \(death\)", "a death with an aura on")
