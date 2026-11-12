// The Jungle rows of tools/catalogue.tsv that the piece picker reads, built into the plugin so the outdoor build needs no file at runtime.
// Columns kept: tileset, builder, leveltype, lvlprest, name, w, h, edgeN, edgeE, edgeS, edgeW, role, weight, tags, check, ds1 (source and hd are dropped). tests/outdoor_tests.cpp compares this text with
// tests/fixtures/jungle_catalogue.tsv, so a catalogue change that is not copied here fails the unit tests.
#include "outdoor_plan.h"

namespace cartographer::outdoor {

namespace {

constexpr const char *kJungleRows[] = {
	"tileset\tbuilder\tleveltype\tlvlprest\tname\tw\th\tedgeN\tedgeE\tedgeS\tedgeW\trole\tweight\ttags\tcheck\tds1\n",
	"Jungle\toutdoor\t21\t530\tAct 3 - Jungle W\t1\t1\tx\tx\tx\tp\tpath\t1\triver,deadend,exits=1\tok\tact3/jungle/rivw.ds1;act3/jungle/rivw2.ds1;act3/jungle/rivw3.ds1\n",
	"Jungle\toutdoor\t21\t531\tAct 3 - Jungle E\t1\t1\tx\tp\tx\tx\tpath\t1\triver,deadend,exits=1\tok\tact3/jungle/rive.ds1;act3/jungle/rive2.ds1;act3/jungle/rive3.ds1\n",
	"Jungle\toutdoor\t21\t532\tAct 3 - Jungle EW\t1\t1\tx\tp\tx\tp\tpath\t1\triver,straight,exits=2\tok\tact3/jungle/rivew.ds1;act3/jungle/rivew2.ds1;act3/jungle/rivew3.ds1\n",
	"Jungle\toutdoor\t21\t533\tAct 3 - Jungle S\t1\t1\tx\tx\tp\tx\tpath\t1\triver,deadend,exits=1\tok\tact3/jungle/rivs.ds1;act3/jungle/rivs2.ds1;act3/jungle/rivs3.ds1\n",
	"Jungle\toutdoor\t21\t534\tAct 3 - Jungle SW\t1\t1\tx\tx\tp\tp\tpath\t1\triver,corner,exits=2\tok\tact3/jungle/rivsw.ds1;act3/jungle/rivsw2.ds1;act3/jungle/rivsw3.ds1\n",
	"Jungle\toutdoor\t21\t535\tAct 3 - Jungle SE\t1\t1\tx\tp\tp\tx\tpath\t1\triver,corner,exits=2\tok\tact3/jungle/rivse.ds1;act3/jungle/rivse2.ds1;act3/jungle/rivse3.ds1\n",
	"Jungle\toutdoor\t21\t536\tAct 3 - Jungle SEW\t1\t1\tx\tp\tp\tp\tpath\t1\triver,tee,exits=3\tok\tact3/jungle/rivsew.ds1;act3/jungle/rivsew2.ds1;act3/jungle/rivsew3.ds1\n",
	"Jungle\toutdoor\t21\t537\tAct 3 - Jungle N\t1\t1\tp\tx\tx\tx\tpath\t1\triver,deadend,exits=1\tok\tact3/jungle/rivn.ds1;act3/jungle/rivn2.ds1;act3/jungle/rivn3.ds1\n",
	"Jungle\toutdoor\t21\t538\tAct 3 - Jungle NW\t1\t1\tp\tx\tx\tp\tpath\t1\triver,corner,exits=2\tok\tact3/jungle/rivnw.ds1;act3/jungle/rivnw2.ds1;act3/jungle/rivnw3.ds1\n",
	"Jungle\toutdoor\t21\t539\tAct 3 - Jungle NE\t1\t1\tp\tp\tx\tx\tpath\t1\triver,corner,exits=2\tok\tact3/jungle/rivne.ds1;act3/jungle/rivne2.ds1;act3/jungle/rivne3.ds1\n",
	"Jungle\toutdoor\t21\t540\tAct 3 - Jungle NEW\t1\t1\tp\tp\tx\tp\tpath\t1\triver,tee,exits=3\tok\tact3/jungle/rivnew.ds1;act3/jungle/rivnew2.ds1;act3/jungle/rivnew3.ds1\n",
	"Jungle\toutdoor\t21\t541\tAct 3 - Jungle NS\t1\t1\tp\tx\tp\tx\tpath\t1\triver,straight,exits=2\tok\tact3/jungle/rivns.ds1;act3/jungle/rivns2.ds1;act3/jungle/rivns3.ds1;act3/jungle/rivns4.ds1;act3/jungle/rivns5.ds1\n",
	"Jungle\toutdoor\t21\t542\tAct 3 - Jungle NSW\t1\t1\tp\tx\tp\tp\tpath\t1\triver,tee,exits=3\tok\tact3/jungle/rivnsw.ds1;act3/jungle/rivnsw2.ds1;act3/jungle/rivnsw3.ds1\n",
	"Jungle\toutdoor\t21\t543\tAct 3 - Jungle NSE\t1\t1\tp\tp\tp\tx\tpath\t1\triver,tee,exits=3\tok\tact3/jungle/rivnse.ds1;act3/jungle/rivnse2.ds1;act3/jungle/rivnse3.ds1\n",
	"Jungle\toutdoor\t21\t544\tAct 3 - Jungle NSEW\t1\t1\tp\tp\tp\tp\tpath\t1\triver,cross,exits=4\tok\tact3/jungle/rivnsew.ds1;act3/jungle/rivnsew2.ds1;act3/jungle/rivnsew3.ds1\n",
	"Jungle\toutdoor\t21\t545\tAct 3 - Jungle W E\t1\t1\tx\tp\tx\tp\tborder\t1\ttransition,woe\tok\tact3/jungle/rivwoe.ds1\n",
	"Jungle\toutdoor\t21\t546\tAct 3 - Jungle W S\t1\t1\tx\tx\tp\tp\tborder\t1\ttransition,wos\tok\tact3/jungle/rivwos.ds1\n",
	"Jungle\toutdoor\t21\t547\tAct 3 - Jungle W N\t1\t1\tp\tx\tx\tp\tborder\t1\ttransition,won\tok\tact3/jungle/rivwon.ds1\n",
	"Jungle\toutdoor\t21\t548\tAct 3 - Jungle E W\t1\t1\tx\tp\tx\tp\tborder\t1\ttransition,eow\tok\tact3/jungle/riveow.ds1\n",
	"Jungle\toutdoor\t21\t549\tAct 3 - Jungle E S\t1\t1\tx\tp\tp\tx\tborder\t1\ttransition,eos\tok\tact3/jungle/riveos.ds1\n",
	"Jungle\toutdoor\t21\t550\tAct 3 - Jungle E N\t1\t1\tp\tp\tx\tx\tborder\t1\ttransition,eon\tok\tact3/jungle/riveon.ds1\n",
	"Jungle\toutdoor\t21\t551\tAct 3 - Jungle EW S\t1\t1\tx\tp\tp\tp\tborder\t1\ttransition,ewos\tok\tact3/jungle/rivewos.ds1\n",
	"Jungle\toutdoor\t21\t552\tAct 3 - Jungle EW N\t1\t1\tp\tp\tx\tp\tborder\t1\ttransition,ewon\tok\tact3/jungle/rivewon.ds1\n",
	"Jungle\toutdoor\t21\t553\tAct 3 - Jungle S W\t1\t1\tx\tx\tp\tp\tborder\t1\ttransition,sow\tok\tact3/jungle/rivsow.ds1\n",
	"Jungle\toutdoor\t21\t554\tAct 3 - Jungle S E\t1\t1\tx\tp\tp\tx\tborder\t1\ttransition,soe\tok\tact3/jungle/rivsoe.ds1\n",
	"Jungle\toutdoor\t21\t555\tAct 3 - Jungle S N\t1\t1\tp\tx\tp\tx\tborder\t1\ttransition,son\tok\tact3/jungle/rivson.ds1\n",
	"Jungle\toutdoor\t21\t556\tAct 3 - Jungle SW E\t1\t1\tx\tp\tp\tp\tborder\t1\ttransition,swoe\tok\tact3/jungle/rivswoe.ds1\n",
	"Jungle\toutdoor\t21\t557\tAct 3 - Jungle SW N\t1\t1\tp\tx\tp\tp\tborder\t1\ttransition,swon\tok\tact3/jungle/rivswon.ds1\n",
	"Jungle\toutdoor\t21\t558\tAct 3 - Jungle SE W\t1\t1\tx\tp\tp\tp\tborder\t1\ttransition,seow\tok\tact3/jungle/rivseow.ds1\n",
	"Jungle\toutdoor\t21\t559\tAct 3 - Jungle SE N\t1\t1\tp\tp\tp\tx\tborder\t1\ttransition,seon\tok\tact3/jungle/rivseon.ds1\n",
	"Jungle\toutdoor\t21\t560\tAct 3 - Jungle SEW N\t1\t1\tp\tp\tp\tp\tborder\t1\ttransition,sewon\tok\tact3/jungle/rivsewon.ds1\n",
	"Jungle\toutdoor\t21\t561\tAct 3 - Jungle N W\t1\t1\tp\tx\tx\tp\tborder\t1\ttransition,now\tok\tact3/jungle/rivnow.ds1\n",
	"Jungle\toutdoor\t21\t562\tAct 3 - Jungle N E\t1\t1\tp\tp\tx\tx\tborder\t1\ttransition,noe\tok\tact3/jungle/rivnoe.ds1\n",
	"Jungle\toutdoor\t21\t563\tAct 3 - Jungle N S\t1\t1\tp\tx\tp\tx\tborder\t1\ttransition,nos\tok\tact3/jungle/rivnos.ds1\n",
	"Jungle\toutdoor\t21\t564\tAct 3 - Jungle NW E\t1\t1\tp\tp\tx\tp\tborder\t1\ttransition,nwoe\tok\tact3/jungle/rivnwoe.ds1\n",
	"Jungle\toutdoor\t21\t565\tAct 3 - Jungle NW S\t1\t1\tp\tx\tp\tp\tborder\t1\ttransition,nwos\tok\tact3/jungle/rivnwos.ds1\n",
	"Jungle\toutdoor\t21\t566\tAct 3 - Jungle NE W\t1\t1\tp\tp\tx\tp\tborder\t1\ttransition,neow\tok\tact3/jungle/rivneow.ds1\n",
	"Jungle\toutdoor\t21\t567\tAct 3 - Jungle NE S\t1\t1\tp\tp\tp\tx\tborder\t1\ttransition,neos\tok\tact3/jungle/rivneos.ds1\n",
	"Jungle\toutdoor\t21\t568\tAct 3 - Jungle NEW S\t1\t1\tp\tp\tp\tp\tborder\t1\ttransition,newos\tok\tact3/jungle/rivnewos.ds1\n",
	"Jungle\toutdoor\t21\t569\tAct 3 - Jungle NS W\t1\t1\tp\tx\tp\tp\tborder\t1\ttransition,nsow\tok\tact3/jungle/rivnsow.ds1\n",
	"Jungle\toutdoor\t21\t570\tAct 3 - Jungle NS E\t1\t1\tp\tp\tp\tx\tborder\t1\ttransition,nsoe\tok\tact3/jungle/rivnsoe.ds1\n",
	"Jungle\toutdoor\t21\t571\tAct 3 - Jungle NSW E\t1\t1\tp\tp\tp\tp\tborder\t1\ttransition,nswoe\tok\tact3/jungle/rivnswoe.ds1\n",
	"Jungle\toutdoor\t21\t572\tAct 3 - Jungle NSE W\t1\t1\tp\tp\tp\tp\tborder\t1\ttransition,nseow\tok\tact3/jungle/rivnseow.ds1\n",
	"Jungle\toutdoor\t21\t573\tAct 3 - Jungle Head\t2\t1\txp\tx\tpx\tx\tborder\t1\ttransition,head\tvariants\tact3/jungle/transl.ds1;act3/jungle/transu.ds1\n",
	"Jungle\toutdoor\t21\t574\tAct 3 - Jungle Tail\t2\t1\tpp\tx\txp\tx\tborder\t1\ttransition,tail\tvariants\tact3/jungle/travl.ds1;act3/jungle/travu.ds1\n",
	"Jungle\toutdoor\t21\t575\tAct 3 - Clearing Webby W\t1\t1\tx\tx\tx\tp\tclearing\t1\tclearing,deadend,exits=1,webby\tok\tact3/jungle/spidw.ds1;act3/jungle/spidw2.ds1;act3/jungle/spidw3.ds1\n",
	"Jungle\toutdoor\t21\t576\tAct 3 - Clearing Webby E\t1\t1\tx\tp\tx\tx\tclearing\t1\tclearing,deadend,exits=1,webby\tok\tact3/jungle/spide.ds1;act3/jungle/spide2.ds1;act3/jungle/spide3.ds1\n",
	"Jungle\toutdoor\t21\t577\tAct 3 - Clearing Webby EW\t1\t1\tx\tp\tx\tp\tclearing\t1\tclearing,straight,exits=2,webby\tok\tact3/jungle/spidew.ds1;act3/jungle/spidew2.ds1;act3/jungle/spidew3.ds1\n",
	"Jungle\toutdoor\t21\t578\tAct 3 - Clearing Webby S\t1\t1\tx\tx\tp\tx\tclearing\t1\tclearing,deadend,exits=1,webby\tok\tact3/jungle/spids.ds1;act3/jungle/spids2.ds1;act3/jungle/spids3.ds1\n",
	"Jungle\toutdoor\t21\t579\tAct 3 - Clearing Webby SW\t1\t1\tx\tx\tp\tp\tclearing\t1\tclearing,corner,exits=2,webby\tok\tact3/jungle/spidsw.ds1;act3/jungle/spidsw2.ds1;act3/jungle/spidsw3.ds1\n",
	"Jungle\toutdoor\t21\t580\tAct 3 - Clearing Webby SE\t1\t1\tx\tp\tp\tx\tclearing\t1\tclearing,corner,exits=2,webby\tok\tact3/jungle/spidse.ds1;act3/jungle/spidse2.ds1;act3/jungle/spidse3.ds1\n",
	"Jungle\toutdoor\t21\t581\tAct 3 - Clearing Webby N\t1\t1\tp\tx\tx\tx\tclearing\t1\tclearing,deadend,exits=1,webby\tok\tact3/jungle/spidn.ds1;act3/jungle/spidn2.ds1;act3/jungle/spidn3.ds1\n",
	"Jungle\toutdoor\t21\t582\tAct 3 - Clearing Webby NW\t1\t1\tp\tx\tx\tp\tclearing\t1\tclearing,corner,exits=2,webby\tok\tact3/jungle/spidnw.ds1;act3/jungle/spidnw2.ds1;act3/jungle/spidnw3.ds1\n",
	"Jungle\toutdoor\t21\t583\tAct 3 - Clearing Webby NE\t1\t1\tp\tp\tx\tx\tclearing\t1\tclearing,corner,exits=2,webby\tok\tact3/jungle/spidne.ds1;act3/jungle/spidne2.ds1;act3/jungle/spidne3.ds1\n",
	"Jungle\toutdoor\t21\t584\tAct 3 - Clearing Webby NS\t1\t1\tp\tx\tp\tx\tclearing\t1\tclearing,straight,exits=2,webby\tok\tact3/jungle/spidns.ds1;act3/jungle/spidns2.ds1;act3/jungle/spidns3.ds1\n",
	"Jungle\toutdoor\t21\t585\tAct 3 - Clearing Boggy W\t1\t1\tx\tx\tx\tp\tclearing\t1\tclearing,deadend,exits=1,boggy\tok\tact3/jungle/bogw.ds1;act3/jungle/bogw2.ds1;act3/jungle/bogw3.ds1\n",
	"Jungle\toutdoor\t21\t586\tAct 3 - Clearing Boggy E\t1\t1\tx\tp\tx\tx\tclearing\t1\tclearing,deadend,exits=1,boggy\tok\tact3/jungle/boge.ds1;act3/jungle/boge2.ds1;act3/jungle/boge3.ds1\n",
	"Jungle\toutdoor\t21\t587\tAct 3 - Clearing Boggy EW\t1\t1\tx\tp\tx\tp\tclearing\t1\tclearing,straight,exits=2,boggy\tok\tact3/jungle/bogew.ds1;act3/jungle/bogew2.ds1;act3/jungle/bogew3.ds1\n",
	"Jungle\toutdoor\t21\t588\tAct 3 - Clearing Boggy S\t1\t1\tx\tx\tp\tx\tclearing\t1\tclearing,deadend,exits=1,boggy\tok\tact3/jungle/bogs.ds1;act3/jungle/bogs2.ds1;act3/jungle/bogs3.ds1\n",
	"Jungle\toutdoor\t21\t589\tAct 3 - Clearing Boggy SW\t1\t1\tx\tx\tp\tp\tclearing\t1\tclearing,corner,exits=2,boggy\tok\tact3/jungle/bogsw.ds1;act3/jungle/bogsw2.ds1;act3/jungle/bogsw3.ds1\n",
	"Jungle\toutdoor\t21\t590\tAct 3 - Clearing Boggy SE\t1\t1\tx\tp\tp\tx\tclearing\t1\tclearing,corner,exits=2,boggy\tok\tact3/jungle/bogse.ds1;act3/jungle/bogse2.ds1;act3/jungle/bogse3.ds1\n",
	"Jungle\toutdoor\t21\t591\tAct 3 - Clearing Boggy N\t1\t1\tp\tx\tx\tx\tclearing\t1\tclearing,deadend,exits=1,boggy\tok\tact3/jungle/bogn.ds1;act3/jungle/bogn2.ds1;act3/jungle/bogn3.ds1\n",
	"Jungle\toutdoor\t21\t592\tAct 3 - Clearing Boggy NW\t1\t1\tp\tx\tx\tp\tclearing\t1\tclearing,corner,exits=2,boggy\tok\tact3/jungle/bognw.ds1;act3/jungle/bognw2.ds1;act3/jungle/bognw3.ds1\n",
	"Jungle\toutdoor\t21\t593\tAct 3 - Clearing Boggy NE\t1\t1\tp\tp\tx\tx\tclearing\t1\tclearing,corner,exits=2,boggy\tok\tact3/jungle/bogne.ds1;act3/jungle/bogne2.ds1;act3/jungle/bogne3.ds1\n",
	"Jungle\toutdoor\t21\t594\tAct 3 - Clearing Boggy NS\t1\t1\tp\tx\tp\tx\tclearing\t1\tclearing,straight,exits=2,boggy\tok\tact3/jungle/bogns.ds1;act3/jungle/bogns2.ds1;act3/jungle/bogns3.ds1\n",
	"Jungle\toutdoor\t21\t595\tAct 3 - Clearing Pygmy W\t1\t1\tx\tx\tx\tp\tclearing\t1\tclearing,deadend,exits=1,pygmy\tok\tact3/jungle/pygw.ds1;act3/jungle/pygw2.ds1;act3/jungle/pygw3.ds1\n",
	"Jungle\toutdoor\t21\t596\tAct 3 - Clearing Pygmy E\t1\t1\tx\tp\tx\tx\tclearing\t1\tclearing,deadend,exits=1,pygmy\tok\tact3/jungle/pyge.ds1;act3/jungle/pyge2.ds1;act3/jungle/pyge3.ds1\n",
	"Jungle\toutdoor\t21\t597\tAct 3 - Clearing Pygmy EW\t1\t1\tx\tp\tx\tp\tclearing\t1\tclearing,straight,exits=2,pygmy\tok\tact3/jungle/pygew.ds1;act3/jungle/pygew2.ds1;act3/jungle/pygew3.ds1\n",
	"Jungle\toutdoor\t21\t598\tAct 3 - Clearing Pygmy S\t1\t1\tx\tx\tp\tx\tclearing\t1\tclearing,deadend,exits=1,pygmy\tok\tact3/jungle/pygs.ds1;act3/jungle/pygs2.ds1;act3/jungle/pygs3.ds1\n",
	"Jungle\toutdoor\t21\t599\tAct 3 - Clearing Pygmy SW\t1\t1\tx\tx\tp\tp\tclearing\t1\tclearing,corner,exits=2,pygmy\tok\tact3/jungle/pygsw.ds1;act3/jungle/pygsw2.ds1;act3/jungle/pygsw3.ds1\n",
	"Jungle\toutdoor\t21\t600\tAct 3 - Clearing Pygmy SE\t1\t1\tx\tp\tp\tx\tclearing\t1\tclearing,corner,exits=2,pygmy\tok\tact3/jungle/pygse.ds1;act3/jungle/pygse2.ds1;act3/jungle/pygse3.ds1\n",
	"Jungle\toutdoor\t21\t601\tAct 3 - Clearing Pygmy N\t1\t1\tp\tx\tx\tx\tclearing\t1\tclearing,deadend,exits=1,pygmy\tok\tact3/jungle/pygn.ds1;act3/jungle/pygn2.ds1;act3/jungle/pygn3.ds1\n",
	"Jungle\toutdoor\t21\t602\tAct 3 - Clearing Pygmy NW\t1\t1\tp\tx\tx\tp\tclearing\t1\tclearing,corner,exits=2,pygmy\tok\tact3/jungle/pygnw.ds1;act3/jungle/pygnw2.ds1;act3/jungle/pygnw3.ds1\n",
	"Jungle\toutdoor\t21\t603\tAct 3 - Clearing Pygmy NE\t1\t1\tp\tp\tx\tx\tclearing\t1\tclearing,corner,exits=2,pygmy\tok\tact3/jungle/pygne.ds1;act3/jungle/pygne2.ds1;act3/jungle/pygne3.ds1\n",
	"Jungle\toutdoor\t21\t604\tAct 3 - Clearing Pygmy NS\t1\t1\tp\tx\tp\tx\tclearing\t1\tclearing,straight,exits=2,pygmy\tok\tact3/jungle/pygns.ds1;act3/jungle/pygns2.ds1;act3/jungle/pygns3.ds1\n",
};

} // namespace

std::string builtinJungleCatalogueText()
{
	std::string text;
	for (const char *row : kJungleRows) text += row;
	return text;
}

} // namespace cartographer::outdoor
