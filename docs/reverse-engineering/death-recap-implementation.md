# Death Recap native adapter

2026-11-27. Consumer: `plugins/death-recap`. This implements the narrow capture paths in
[the static contracts](death-recap-static.md). It is an alpha implementation, with incomplete mechanic coverage.
The initial pass skipped game checks. A subsequently authorized [narrow smoke check](../../plugins/death-recap/TESTING.md) verified loading/coexistence and synthetic UI appearance without a character death; combat capture remains unverified.

## Checked image

Memory-layout image: 90,451,968 bytes, stamp `0x6ab3782c`, SHA-256
`c4f97dbda72e6d213494d589788a3121abba8eefc62a1dbe29c785781f38f30f`.
This differs from the hash in the earlier static report. All 17 source signatures/witnesses below pass against
this image. The checker reports the hash rather than assuming the images are identical.

Run `python plugins/death-recap/tools/verify_dump.py <memory-layout-dump>` with Capstone installed.
It reads files only, scans executable PE sections, requires unique matches, checks the original four E8 targets,
and disassembles every overwritten entry to verify complete, non-relative instructions. There is no image-stamp gate.

## Enabled parts and ownership

| Part | Location in this image | Mutation and guard |
|---|---|---|
| Direct HP/mana commits | `0x44d093`, `0x44d0c9` | Five-byte E8 replacements, unique surrounding patterns, target must equal independently identified SetStat thunk `0x2f7d10` |
| HP/mana regeneration commits | `0x42e6d1`, `0x42e540` | Same, with SetStat / AddStat `0x2f34f0` respectively; endpoints only, no gross regeneration or poison claim |
| Player mode dispatch | `0x42d020` | Unique entry, 17 whole bytes, original eight arguments and return preserved; only living -> mode 0/17 after a mode-0 request is a death |
| Optional hit context | `0x44ce80` | Unique entry, 17 whole bytes; does not install when PD2 or another owner has changed its entry |
| Optional effect stages | `0x44df10` | Unique entry, 21 whole bytes; copied only into matching bounded hit context |
| Optional healing | `0x451820` | Unique entry, 15 whole bytes; actual endpoints, original return preserved |

Each part has its own SDK mutation transaction and load-log result. SDK conflicts or a changed/missing signature leave
that part off. SetStat, AddStat and GetStat are only called through their existing D2RCore thunks; no thunk slot is
written. GetStat, SetStat and AddStat have independent masked entry/body witnesses in `src/signatures.h`, preventing
a foreign E8 target from being accepted merely because the surrounding call-site pattern still matches.
No mutation is made to DealDamage, absorption's specific call site, missile/area entries, or regen function entries.

The E8 replacements use four sixteen-byte absolute-jump relays in a page allocated within rel32 range at load.
The page remains allocated until process exit because the SDK restores mutations after the unload callback returns.
This intentionally retains one 4 KiB page per plugin load, rather than freeing a target while its call is still installed.
The original functions are called exactly once, with unchanged arguments and RAX/result returned to the caller.

## Identity, ordering and coverage

SDK LocalPlayerReady / PlayerResurrected / GameLeft listeners publish a per-life token and local unit ID. A queued
authoritative game task binds the producer thread for that token. Matching checks require player type, ID, bound
identity and thread. The documented unit fields are type `+0`, class `+4`, ID `+8`, mode `+0xc`; the data bank comes
from the game at `+0x106`. No unit pointer is retained in a report.

The producer copies POD events into a bounded SPSC feed. The UI consumer alone owns Rolling/Freeze, allocating reports
only after the producer has enqueued its death marker. A bounded thread-local hit stack carries attacker and effect
metadata. Stack overflow loses attribution instead of overwriting a parent context. An enclosing write that observed
nested mutations publishes an endpoint sample, preserving the nested gross amounts without adding them again.
The final attempted direct HP debit is sampled before the original setter. Fractional flush and overkill remain separate.
Mana events never inherit the HP damage block's element totals. Poison rate is excluded from immediate element damage.

Mode dispatch copies defenses before cleanup and checks mode after the original. The death marker waits for any
enclosing observed hit to return and for its final endpoints. Queue saturation leaves sequence gaps; a pending death
marker is retried on the producer. Mode 17, repeated death dispatch and remote unit IDs do not create another death.
The UI also deduplicates reports per life. On a joined client, or when native death capture is unavailable, client
snapshots provide an explicitly replicated partial report only after an observed living-to-dead transition. Switching
feeds resets accounting rather than mixing independent sequence streams.

These are narrow observations, not complete authoritative telemetry. Persistent damage, Crushing Blow, other custom
writers, Energy Shield, costs, poison/OW attribution, missile provenance, pet owners and attacker modifiers remain
unclassified or unavailable. Snapshot gaps contribute unresolved net changes. A last direct hit is not promoted to a
confirmed killing blow. Effect stages calculated outside the observed hit scope remain unavailable. Reconciliation
does not establish coverage; every native report remains marked partial.

## Table names, states and panel witnesses

The MonStats loader at `0x39dbdb` references `NameStr` at `0x1d06310` and writes its descriptor to `[rbp+0x40]`.
Instructions `0x39dccc` / `0x39dcd4` store descriptor type `0x18` and row offset `0x36`; their sixteen-byte witness is
unique. The adapter requires that witness, an SDK MonStats row size of `0x1fc` and the row's u16 index matching its
position before reading the u16 string reference at `+0x36`. Localization lookup failures fall back to the active
compiled `monstats:<Id>` key (`+2`, bounded 47 bytes), rather than a vanilla name list. These field interpretations
are statically checked, not verified live with D2RCore's table compiler. A table-load callback copies names from each
available SDK data bank and copies only report-used names into the owned report, so history never resolves against a
different active mod. New plugin/mod localization strings unavailable through numeric SDK lookup use the key fallback.

The same game-thread callback copies States table bounds (maximum 1024); a uniquely identified `HasState` at `0x3351b0`
captures up to sixteen active states before death cleanup, with an explicit truncation flag. Missing getter/table bounds
leave state evidence unavailable. Defense stats 36/37/39..45 remain in native stat units, distinct from 1/256 resources.

Client slot/player getters come from the existing Player Bars ExpBar witness (`0x14cc8c0`, calls +`0x1b` and +`0x22`).
FindPanel `0x846170`, FindChild `0x856220`, and TextBox SetText `0x86dd50` use the documented Player Bars/DPS Counter
witnesses. SetText is identified at entry +16 so an existing compatible entry hook is called through, not overwritten.
Plugin-owned HP sample rectangles use the documented widget rect at `+0x70`, after checking their declared width.
No stock widget is changed. All panel operations run on the UI thread via SDK services. The later smoke check widened the owned panel and graph to contain its text at 1920x1080.

The pinned Input service has keyboard actions only. Controller reopen uses the optional SDK controller route; conflicts
leave that route off. System XInput observes D-pad/B edges only while the recap is open, without intercepting gameplay
or revive controls. The later smoke check verified native panel rendering, keyboard dismissal/reopen and timeline paging. Controller interaction and revive coexistence remain unverified.

## Validation

- Windows x64 DLL compiled with installed VS2019/MSVC 19.29 and Windows SDK 10.0.19041. VS2022 is not installed here.
- Three host test executables cover telemetry/summary, SPSC/settings/persistence, and the actual native wrapper code
  against fake units/originals. The latter checks original call counts and returns, fractional flush, overkill, nested
  writes, mode-17 deduplication, remote identity, foreign threads, state truncation and a saturated death feed.
- The dump checker passes all capture and auxiliary witnesses on the image identified above.
- Initial pass: no installation, offline game, TCP/IP host/join, mechanic matrix, UI rendering, controller, or coexistence game check. The later [smoke results](../../plugins/death-recap/TESTING.md) supersede only the narrow loading/UI/coexistence portions.
  Those remain the validation gates before release. The code does not establish full handoff acceptance.


## Native recap presentation (2026-11-27)

The mockup-driven presentation remains separate from capture: `view.h` derives display data from a const report;
`panel.cpp` registers plugin-owned native layouts/resources and updates them only on the UI thread. No overlay draw
commands, browser surface, mod layout dependency, game asset copy, or game binary is shipped. The overlay callback
continues to serve only as a UI-thread clock. Layout coordinates use D2R's 2160-high HD space and centered anchors.

Player Bars' independently signature-identified widget screen-position function (`0x8562f0`) and image clip witness
(`0x858d1a`, inside AbstractImageWidget::DrawFrame `0x858c50`) are reused as plugin-local patterns. The latter's first
instruction supplies the clip offset, required to equal `0xac`. The native clip is `{x,y,width,height}` in screen
pixels. Three stock `PANEL/Modals/Modal_No_Title_BG` image widgets, uniformly scaled, provide the textured frame;
clipping removes their repeated top/bottom ornament. The screen-space scale is measured between the panel origin
and an owned marker. Clips are refreshed while open, including after resolution/UI-scale changes. TooltipBorder
nine-slice widgets provide section edges; ModalButton provides the Close texture/states. The rect guard checks the
complete declared or last-written `{x,y,width,height}` at `+0x70` before changing an owned widget. A mismatch leaves
presentation updates off, with a diagnostic; no gameplay or stock widget is mutated.

The current loader refuses further widget paths after 256 handles (observed at graph rectangle Heal51). Graph and
bar rectangles therefore use their guarded owned rect directly; zero extents hide unused rectangles. Other widgets
use the SDK visibility service. The graph is a step trace of confirmed resource endpoints: no interpolation across
sequence gaps, unknown endpoints, or intervals over 500ms. Its maximum includes observed maxima and endpoints;
healing markers require measured gross restoration. Graph columns are aligned to physical pixels; adjacent equal-height fills and flat trace runs are combined to avoid fractional-scale artifacts. Native text ellipsis contains long labels, while advanced text wraps across pages. Element percentages divide measured element subtotals, never
actual life loss. Missing elements, sources, ability names and fatal links remain unavailable.

Native buttons use MultiSpec's proven `PanelManager:OpenPanel:death-recap/<action>` route. A SharedEvent listener
consumes only those names, queues the action and applies it in the next UI task. Mouse-wheel capture uses a removable
Win32 window subclass only while the recap is open; keyboard and XInput edges provide section, scroll, page and
close operations. New reports/reopening reset expansion. Automatic opening waits for PlayerResurrected; the stock
YouDiedModal and its Continue action are untouched. A physical controller has not been validated.

Death context adds stat 12 (level, already verified by Better XP Bar) and the optional pinned SDK supported export
GetLevelIdFromUnit (ordinal 2000) to the pre-cleanup death snapshot. No game pointer enters the immutable report.
The Levels loader's `LevelName` descriptor at `0x33024d` sets type 1, length 39, offset `0xfd`; the unique witness is
checked before reading a bounded key from SDK Levels rows. SDK key localization is copied at table load; only the
report's area name is retained. Live 3.4 deaths confirmed Harrogath and Blood Moor. Missing export/layout/localization
leaves the area unavailable. History format 2 adds these optional fields and retains format-1 decoding.

Defense capture also copies stat IDs 34/35 (flat physical/magic reduction), 142/143 (percent/flat fire absorption),
144/145 (percent/flat lightning absorption), and 148 (percent cold absorption), within the existing 16-stat bound.
Their meanings come from active 95032 itemstatcost data; displayed percentages remain native stat units. Uncaptured
mechanics and state-name/category mappings are explicitly unavailable rather than inferred.

`tools/verify_dump.py` now verifies 20 unique signatures/witnesses on the same documented image, including the three
new UI/context witnesses. Target engine identity remains `0x6ab3782c` for 3.3/3.4; no version gate was introduced.
Live validation was on 3.4 only; see the plugin TESTING.md for exact scenarios, screenshots and limitations.
