#include "../src/requirements.h"
#include <cstdlib>
#include <iostream>
using namespace armory;
void Check(bool ok){if(!ok) std::abort();}
int main(){
    Attributes base{20,20,50,1};
    std::array<GearRequirement,2> gear{{{1,20,20,1,1,10,0,true},{2,30,20,1,1,0,10,true}}};
    Check(CheckRequirements(base,gear)==RequirementResult::Satisfied);
    gear[0].dexterity=30; Check(CheckRequirements(base,gear)==RequirementResult::Dependency);
    gear[0].dexterity=20; gear[0].strength=30; Check(CheckRequirements(base,gear)==RequirementResult::Dependency);
    gear[0].strength=20; gear[0].slot=4; Check(CheckRequirements(base,gear)==RequirementResult::Dependency);
    gear[0].slot=1; gear[1].slot=11; Check(CheckRequirements(base,gear)==RequirementResult::Satisfied);
    gear[1].level=51; Check(CheckRequirements(base,gear)==RequirementResult::Illegal); gear[1].level=50;
    gear[1].classes=2; Check(CheckRequirements(base,gear)==RequirementResult::Illegal); gear[1].classes=1;
    gear[1].known=false; Check(CheckRequirements(base,gear)==RequirementResult::Unknown); gear[1].known=true;
    gear[1].grantsDexterity=-1; Check(CheckRequirements(base,gear)==RequirementResult::Unknown); gear[1].grantsDexterity=0;
    gear[1].slot=1; Check(CheckRequirements(base,gear)==RequirementResult::Illegal);
    std::cout<<"Armory conservative requirement checks passed\n";
}
