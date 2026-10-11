#include "AetheriaRules.h"
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>
using namespace Aetheria::Rules;

namespace {
int checks=0,failures=0;
#define CHECK(expression) do { ++checks; if(!(expression)){++failures;std::cerr<<__FILE__<<":"<<__LINE__<<" failed: "<<#expression<<"\n";} } while(false)
std::int64_t TotalCoins(const State& s){return s.wallet+s.bank+s.merchantTreasury+s.rewardTreasury;}
std::string Snapshot(const State& s) {
    const auto bytes=EncodeSave(s);std::string value(bytes.begin(),bytes.end());
    value+=std::to_string(s.serverTimeMs)+"/"+std::to_string(s.lastCastAtMs)+"/"+std::to_string(s.pendingCasts.size());
    for(const auto& p:s.pendingCasts)value+=std::to_string(p.id)+":"+std::to_string(p.resolved);
    return value;
}
State Awakened(std::uint32_t random=0) {
    State s=CreateNewPlayer("Aerin");
    CHECK(ApplyStoryEvent(s,StoryEvent::MeetDoctor).ok);
    CHECK(ApplyStoryEvent(s,StoryEvent::MeetSeraphine).ok);
    CHECK(Awaken(s,random).ok);
    return s;
}
void Trial(State& s) {
    const auto spell=DefaultSpell(s.affinity);
    for(int i=0;i<3;++i){
        CHECK(Tick(s,1000).ok);
        auto cast=Cast(s,spell,s.serverTimeMs);CHECK(cast.ok);
        CHECK(ConsumeTrialCast(s,cast.castId,StoryEvent::TrialTargetHit,s.serverTimeMs).ok);
    }
    CHECK(SpendStamina(s,20).ok);
    CHECK(ApplyStoryEvent(s,StoryEvent::TrialDodge).ok);
    CHECK(Tick(s,1000).ok);
    auto interaction=Cast(s,spell,s.serverTimeMs);CHECK(interaction.ok);
    CHECK(ConsumeTrialCast(s,interaction.castId,StoryEvent::TrialElementInteraction,s.serverTimeMs).ok);
    CHECK(s.chapter==Chapter::GuardianEncounter);
}
void DefeatGuardian(State& s) {
    GuardianState g=CreateGuardian();bool phaseTwo=false;int count=0;
    while(g.phase!=GuardianPhase::Defeated&&count++<50) {
        CHECK(Tick(s,5000).ok);
        auto cast=Cast(s,DefaultSpell(s.affinity),s.serverTimeMs);CHECK(cast.ok);
        if(!cast.ok)break;
        auto hit=ResolveGuardianHit(s,g,cast.castId,s.serverTimeMs);CHECK(hit.ok);
        if(g.phase==GuardianPhase::Two)phaseTwo=true;
    }
    CHECK(phaseTwo);CHECK(g.phaseTwoReached);CHECK(g.health==0);
    CHECK(s.guardianDefeated);CHECK(s.chapter==Chapter::CityVisits);
}
void VisitCity(State& s) {
    for(unsigned e=static_cast<unsigned>(StoryEvent::VisitHospital);e<=static_cast<unsigned>(StoryEvent::VisitInn);++e)
        CHECK(ApplyStoryEvent(s,static_cast<StoryEvent>(e)).ok);
    CHECK(s.visitedFacilities==0x7f);CHECK(s.chapter==Chapter::FinalDecision);
}
State CityState() {auto s=Awakened();Trial(s);DefeatGuardian(s);return s;}
void IdentityAndAwakening() {
    CHECK(ElementCount==11);CHECK(StageCount==6);CHECK(static_cast<unsigned>(Element::Healing)==10);
    auto s=CreateNewPlayer("माया");CHECK(s.affinity==Element::None);CHECK(s.chapter==Chapter::HospitalStart);
    CHECK(s.health==70);CHECK(ValidateState(s));CHECK(GetObjective(s).find("hospital")!=std::string::npos);
    CHECK(CreateNewPlayer(std::string(200,'a')).name=="Aerin");
    const auto before=Snapshot(s);CHECK(!Awaken(s,0).ok);CHECK(Snapshot(s)==before);
    CHECK(!Cast(s,DefaultSpell(Element::Fire),0).ok);CHECK(Snapshot(s)==before);
    for(unsigned i=0;i<ElementCount;++i) {
        const auto sample=static_cast<std::uint32_t>(((2ULL*i+1)*(1ULL<<32))/(2*ElementCount));
        auto player=Awakened(sample);CHECK(player.affinity==static_cast<Element>(i));
        CHECK(player.mastery[i].stage==Stage::Basic);CHECK(player.mastery[i].xp==0);
        CHECK(std::count_if(player.mastery.begin(),player.mastery.end(),[](const Mastery& m){return m.awakened;})==1);
        const auto stable=Snapshot(player);CHECK(!Awaken(player,~sample).ok);CHECK(Snapshot(player)==stable);
        CHECK(ValidateState(player));
    }
    CHECK(Awakened(0).affinity==Element::Fire);
    CHECK(Awakened(std::numeric_limits<std::uint32_t>::max()).affinity==Element::Healing);
}
void StoryOrderAndEveryAffinity() {
    auto s=CreateNewPlayer("Story");
    for(auto event:{StoryEvent::MeetSeraphine,StoryEvent::TrialTargetHit,StoryEvent::GuardianDefeated,StoryEvent::VisitBank,StoryEvent::ReturnToSeraphine}) {
        auto before=Snapshot(s);CHECK(!ApplyStoryEvent(s,event).ok);CHECK(Snapshot(s)==before);
    }
    for(unsigned i=0;i<ElementCount;++i) {
        auto player=Awakened(static_cast<std::uint32_t>(((2ULL*i+1)*(1ULL<<32))/(2*ElementCount)));
        const auto total=TotalCoins(player);Trial(player);DefeatGuardian(player);
        CHECK(TotalCoins(player)==total);
        auto duplicate=Snapshot(player);CHECK(!ApplyStoryEvent(player,StoryEvent::GuardianDefeated).ok);CHECK(Snapshot(player)==duplicate);
        CHECK(ApplyStoryEvent(player,StoryEvent::VisitBank).ok);
        duplicate=Snapshot(player);CHECK(!ApplyStoryEvent(player,StoryEvent::VisitBank).ok);CHECK(Snapshot(player)==duplicate);
        for(auto event:{StoryEvent::VisitHospital,StoryEvent::VisitAcademy,StoryEvent::VisitGuildHall,StoryEvent::VisitHouses,StoryEvent::VisitMarket,StoryEvent::VisitInn})
            CHECK(ApplyStoryEvent(player,event).ok);
        CHECK(player.chapter==Chapter::FinalDecision);
        CHECK(!ChooseFinal(player,FinalChoice::Seal).ok);
        CHECK(ApplyStoryEvent(player,StoryEvent::ReturnToSeraphine).ok);
        auto choice=i%2?FinalChoice::Seal:FinalChoice::Study;CHECK(ChooseFinal(player,choice).ok);
        CHECK(player.chapter==Chapter::Complete);CHECK(player.finalChoice==choice);CHECK(TotalCoins(player)==total);
        duplicate=Snapshot(player);CHECK(!ChooseFinal(player,choice==FinalChoice::Seal?FinalChoice::Study:FinalChoice::Seal).ok);CHECK(Snapshot(player)==duplicate);
        CHECK(ValidateState(player));
    }
}
void BoundedSpellsAndClock() {
    auto s=Awakened();auto p=DefaultSpell(s.affinity);const auto initial=Snapshot(s);
    auto wrong=p;wrong.element=Element::Water;CHECK(!Cast(s,wrong,0).ok);CHECK(Snapshot(s)==initial);
    auto invalid=p;invalid.intensity=std::numeric_limits<float>::infinity();CHECK(!Cast(s,invalid,0).ok);
    invalid=p;invalid.range=std::numeric_limits<float>::quiet_NaN();CHECK(!Cast(s,invalid,0).ok);
    invalid=p;invalid.form=static_cast<Form>(255);CHECK(!Cast(s,invalid,0).ok);
    invalid=p;invalid.intensity=3;CHECK(!Cast(s,invalid,0).ok);CHECK(Snapshot(s)==initial);
    auto first=Cast(s,p,0);CHECK(first.ok);CHECK(first.castId!=0);CHECK(first.manaCost==GetSpellCost(s,p));CHECK(first.damage<=100);
    auto after=Snapshot(s);CHECK(!Cast(s,p,599).ok);CHECK(Snapshot(s)==after);
    auto exact=Cast(s,p,600);CHECK(exact.ok);
    after=Snapshot(s);CHECK(!Cast(s,p,599).ok);CHECK(Snapshot(s)==after);
    CHECK(!Cast(s,p,std::numeric_limits<std::uint64_t>::max()).ok);CHECK(Snapshot(s)==after);
    auto poor=s;poor.mana=0;auto poorBefore=Snapshot(poor);CHECK(!Cast(poor,p,1200).ok);CHECK(Snapshot(poor)==poorBefore);
    auto dead=s;dead.health=0;CHECK(!Cast(dead,p,1200).ok);CHECK(!UseItem(dead,"health-potion").ok);
    auto tired=s;tired.fatigue=80;CHECK(!Cast(tired,p,1200).ok);
    auto weak=s;weak.concentration=29;CHECK(!Cast(weak,p,1200).ok);
    CHECK(!Tick(s,0).ok);CHECK(!Tick(s,60001).ok);
    auto fractional=Awakened();fractional.mana=50;
    CHECK(Tick(fractional,500).ok);CHECK(fractional.mana==50);
    CHECK(Tick(fractional,500).ok);CHECK(fractional.mana==55);
    CHECK(!SpendStamina(fractional,-1).ok);CHECK(!SpendStamina(fractional,101).ok);
    CHECK(SpendStamina(fractional,100).ok);CHECK(fractional.stamina==0);CHECK(!SpendStamina(fractional,1).ok);
}
void CastProofAndGuardianPhases() {
    auto s=Awakened();auto cast=Cast(s,DefaultSpell(s.affinity),0);CHECK(cast.ok);
    CHECK(ConsumeTrialCast(s,cast.castId,StoryEvent::TrialTargetHit,0).ok);
    auto before=Snapshot(s);CHECK(!ConsumeTrialCast(s,cast.castId,StoryEvent::TrialTargetHit,0).ok);CHECK(Snapshot(s)==before);
    CHECK(!ConsumeTrialCast(s,cast.castId,StoryEvent::VisitBank,0).ok);CHECK(Snapshot(s)==before);
    auto expired=Awakened();auto old=Cast(expired,DefaultSpell(expired.affinity),0);CHECK(old.ok);
    before=Snapshot(expired);CHECK(!ConsumeTrialCast(expired,old.castId,StoryEvent::TrialTargetHit,10001).ok);CHECK(Snapshot(expired)==before);
    auto bossPlayer=Awakened();Trial(bossPlayer);
    auto& mastery=bossPlayer.mastery[0];mastery.xp=460;mastery.stage=Stage::Perfect;
    auto p=DefaultSpell(Element::Fire);p.intensity=5;CHECK(Tick(bossPlayer,10000).ok);
    auto large=Cast(bossPlayer,p,bossPlayer.serverTimeMs);CHECK(large.ok);CHECK(large.damage==100);
    auto boss=CreateGuardian();boss.health=130;
    CHECK(ResolveGuardianHit(bossPlayer,boss,large.castId,bossPlayer.serverTimeMs+200).ok);
    CHECK(boss.health==120);CHECK(boss.phase==GuardianPhase::Two);CHECK(boss.phaseTwoReached);
    auto unchanged=boss.health;before=Snapshot(bossPlayer);
    CHECK(!ResolveGuardianHit(bossPlayer,boss,large.castId,bossPlayer.serverTimeMs).ok);
    CHECK(boss.health==unchanged);CHECK(Snapshot(bossPlayer)==before);
    // Hit travel time contributes to cooldown expiry exactly once.
    auto next=Cast(bossPlayer,DefaultSpell(Element::Fire),large.castId?bossPlayer.lastCastAtMs+600:0);CHECK(next.ok);
    auto participant=Awakened();Trial(participant);
    GuardianState fabricated;fabricated.health=0;fabricated.phase=GuardianPhase::Defeated;
    before=Snapshot(participant);CHECK(!ConfirmGuardianDefeat(participant,fabricated).ok);CHECK(Snapshot(participant)==before);
    fabricated.phaseTwoReached=true;const auto total=TotalCoins(participant);
    CHECK(ConfirmGuardianDefeat(participant,fabricated).ok);CHECK(TotalCoins(participant)==total);
    before=Snapshot(participant);CHECK(!ConfirmGuardianDefeat(participant,fabricated).ok);CHECK(Snapshot(participant)==before);
}
void MasteryAndFusion() {
    CHECK(StageForXp(39)==Stage::Basic);CHECK(StageForXp(40)==Stage::Real);CHECK(StageForXp(100)==Stage::High);
    CHECK(StageForXp(180)==Stage::Advanced);CHECK(StageForXp(300)==Stage::Super);CHECK(StageForXp(460)==Stage::Perfect);
    CHECK(StageForXp(std::numeric_limits<std::uint32_t>::max())==Stage::Perfect);
    auto s=Awakened();auto before=Snapshot(s);CHECK(!Train(s,Element::Fire).ok);CHECK(Snapshot(s)==before);
    auto cast=Cast(s,DefaultSpell(Element::Fire),0);CHECK(cast.ok);CHECK(ConsumeTrialCast(s,cast.castId,StoryEvent::TrialTargetHit,0).ok);
    const auto total=TotalCoins(s);CHECK(Train(s,Element::Fire).ok);CHECK(s.mastery[0].stage==Stage::Real);CHECK(TotalCoins(s)==total);
    CHECK(!Train(s,Element::Fire).ok);
    auto future=Awakened();future.level=2;future.mastery[0].xp=180;future.mastery[0].stage=Stage::Advanced;
    auto& earth=future.mastery[static_cast<unsigned>(Element::Earth)];earth.awakened=true;earth.awakenedAtLevel=2;earth.xp=179;earth.stage=Stage::High;
    before=Snapshot(future);CHECK(!Fuse(future,Element::Fire,Element::Earth).ok);CHECK(Snapshot(future)==before);
    earth.xp=180;earth.stage=Stage::Advanced;
    CHECK(Fuse(future,Element::Fire,Element::Earth).ok);CHECK(future.fusions.size()==1);CHECK(future.fusions[0].stage==Stage::Basic);CHECK(future.fusions[0].xp==0);
    before=Snapshot(future);CHECK(!Fuse(future,Element::Earth,Element::Fire).ok);CHECK(Snapshot(future)==before);
    auto p=DefaultSpell(Element::Fire);p.fusionIndex=0;p.intensity=3;CHECK(!ValidateSpell(future,p).ok);
    p.intensity=1;CHECK(ValidateSpell(future,p).ok);CHECK(GetSpellCost(future,p)>GetSpellCost(future,DefaultSpell(Element::Fire)));
    CHECK(ValidateState(future));
    auto impossible=future;impossible.mastery[0].xp=179;impossible.mastery[0].stage=Stage::High;CHECK(!ValidateState(impossible));
}
void ExperimentsAndEconomy() {
    auto s=Awakened();const auto p=DefaultSpell(s.affinity);
    CHECK(!RegisterSpell(s,p,"Sunseed").ok);CHECK(ExperimentSpell(s,p,0).ok);CHECK(s.spells.size()==1);
    const auto id=s.spells[0].id;CHECK(!ReproduceSpell(s,id,1000).ok);
    CHECK(StabilizeSpell(s,id).ok);CHECK(ReproduceSpell(s,id,1000).ok);CHECK(ReproduceSpell(s,id,2000).ok);
    CHECK(s.spells[0].phase==SpellPhase::Reproduced);CHECK(!RegisterSpell(s,p,"bad\nname").ok);
    CHECK(RegisterSpell(s,p,"Sunseed").ok);CHECK(s.spells[0].phase==SpellPhase::Registered);
    CHECK(!ExperimentSpell(s,p,3000).ok);CHECK(ValidateState(s));
    auto unawakened=CreateNewPlayer("Guest");auto initial=Snapshot(unawakened);
    CHECK(!Deposit(unawakened,10).ok);CHECK(!BuyItem(unawakened,"mana-potion",1).ok);CHECK(Snapshot(unawakened)==initial);
    auto city=CityState();const auto total=TotalCoins(city);
    for(auto amount:{std::int64_t(0),std::int64_t(-1),std::numeric_limits<std::int64_t>::max()}) {
        auto before=Snapshot(city);CHECK(!Deposit(city,amount).ok);CHECK(!Withdraw(city,amount).ok);CHECK(Snapshot(city)==before);
    }
    CHECK(Deposit(city,100).ok);CHECK(Withdraw(city,60).ok);CHECK(TotalCoins(city)==total);
    auto before=Snapshot(city);CHECK(!Withdraw(city,100).ok);CHECK(Snapshot(city)==before);
    CHECK(BuyItem(city,"mana-potion",1).ok);CHECK(TotalCoins(city)==total);
    before=Snapshot(city);CHECK(!BuyItem(city,"mana-potion",std::numeric_limits<std::int32_t>::max()).ok);
    CHECK(!BuyItem(city,"unknown-item",1).ok);CHECK(Snapshot(city)==before);
    city.mana=20;CHECK(UseItem(city,"mana-potion").ok);CHECK(city.mana==60);CHECK(city.inventory.empty());
    CHECK(!UseItem(city,"mana-potion").ok);CHECK(TotalCoins(city)==total);
    CHECK(ApplyServerDamage(city,30).ok);CHECK(city.health==70);CHECK(!ApplyServerDamage(city,101).ok);
    CHECK(ValidateState(city));
}
void HospitalAndInnServices() {
    auto opening=CreateNewPlayer("Aerin");
    const auto openingCoins=TotalCoins(opening), openingWallet=opening.wallet;
    CHECK(TreatAtHospital(opening).ok);CHECK(opening.health==100);
    CHECK(opening.wallet==openingWallet);CHECK(TotalCoins(opening)==openingCoins);
    CHECK(ValidateState(opening));
    auto living=Awakened();living.health=70;living.mana=40;living.fatigue=25;
    const auto coins=TotalCoins(living), wallet=living.wallet;
    CHECK(TreatAtHospital(living).ok);CHECK(living.wallet==wallet-HospitalTreatmentGoldCost);
    CHECK(TotalCoins(living)==coins);CHECK(living.health==100);CHECK(living.mana==100);CHECK(living.fatigue==0);
    auto poor=Awakened();poor.health=70;poor.wallet=0;
    auto before=Snapshot(poor);CHECK(!TreatAtHospital(poor).ok);CHECK(Snapshot(poor)==before);
    poor.health=0;poor.inventory.push_back({"health-potion",1});
    before=Snapshot(poor);CHECK(!Cast(poor,DefaultSpell(poor.affinity),poor.serverTimeMs).ok);
    CHECK(!UseItem(poor,"health-potion").ok);CHECK(Snapshot(poor)==before);
    const auto emergencyCoins=TotalCoins(poor);CHECK(TreatAtHospital(poor).ok);
    CHECK(poor.health==50);CHECK(poor.mana==50);CHECK(poor.stamina==50);CHECK(poor.wallet==0);
    CHECK(TotalCoins(poor)==emergencyCoins);CHECK(ValidateState(poor));
    before=Snapshot(living);CHECK(!RestAtInn(living).ok);CHECK(Snapshot(living)==before);
    auto city=CityState();city.health=40;city.mana=30;city.stamina=25;city.fatigue=30;
    const auto cityCoins=TotalCoins(city), cityWallet=city.wallet;
    const auto clock=city.serverTimeMs;
    CHECK(RestAtInn(city).ok);CHECK(city.wallet==cityWallet-InnRestGoldCost);CHECK(TotalCoins(city)==cityCoins);
    CHECK(city.health==70);CHECK(city.mana==100);CHECK(city.stamina==100);CHECK(city.fatigue==0);
    CHECK(city.serverTimeMs==clock);CHECK(ValidateState(city));
    city.health=0;before=Snapshot(city);CHECK(!RestAtInn(city).ok);CHECK(Snapshot(city)==before);
    city.health=20;city.wallet=0;before=Snapshot(city);CHECK(!RestAtInn(city).ok);CHECK(Snapshot(city)==before);
}
void SavePersistenceAndMigration() {
    auto s=CityState();VisitCity(s);CHECK(ApplyStoryEvent(s,StoryEvent::ReturnToSeraphine).ok);CHECK(ChooseFinal(s,FinalChoice::Study).ok);
    s.appearance.sex="male";s.appearance.hairColor="black";s.appearance.bodyScale=1.05f;
    CHECK(BuyItem(s,"health-potion",2).ok);CHECK(Deposit(s,100).ok);
    const auto bytes=EncodeSave(s);CHECK(!bytes.empty());State restored;
    CHECK(DecodeSave(bytes,restored));CHECK(restored.name==s.name);CHECK(restored.finalChoice==FinalChoice::Study);
    CHECK(restored.chapter==Chapter::Complete);CHECK(restored.affinity==s.affinity);CHECK(restored.wallet==s.wallet);
    CHECK(restored.bank==s.bank);CHECK(restored.inventory.size()==1);CHECK(restored.inventory[0].count==2);
    CHECK(restored.appearance.sex=="male");CHECK(restored.appearance.bodyScale==1.05f);CHECK(restored.pendingCasts.empty());CHECK(ValidateState(restored));
    auto broken=bytes;broken[20]^=0x55;const auto before=Snapshot(restored);CHECK(!DecodeSave(broken,restored));CHECK(Snapshot(restored)==before);
    for(std::size_t size=0;size<std::min<std::size_t>(bytes.size(),100);++size) {
        std::vector<std::uint8_t> truncated(bytes.begin(),bytes.begin()+size);CHECK(!DecodeSave(truncated,restored));CHECK(Snapshot(restored)==before);
    }
    auto legacy=Awakened();legacy.version=1;legacy.mastery[0].xp=100;legacy.mastery[0].stage=Stage::Basic;legacy.mastery[0].awakenedAtLevel=0;
    CHECK(MigrateSave(legacy));CHECK(legacy.version==CurrentSaveVersion);CHECK(legacy.mastery[0].stage==Stage::High);CHECK(legacy.mastery[0].awakenedAtLevel==1);
    legacy.version=2;CHECK(MigrateSave(legacy));
    auto future=legacy;future.version=CurrentSaveVersion+1;auto futureVersion=future.version;CHECK(!MigrateSave(future));CHECK(future.version==futureVersion);
    auto invalid=legacy;invalid.wallet=-1;CHECK(EncodeSave(invalid).empty());CHECK(!MigrateSave(invalid));CHECK(invalid.wallet==-1);
    invalid=legacy;invalid.mastery[0].stage=Stage::Perfect;CHECK(!ValidateState(invalid));
    invalid=legacy;invalid.chapter=Chapter::Complete;invalid.finalChoice=FinalChoice::Study;CHECK(!ValidateState(invalid));
    auto cooldown=Awakened();auto cast=Cast(cooldown,DefaultSpell(cooldown.affinity),0);CHECK(cast.ok);
    State loaded;CHECK(DecodeSave(EncodeSave(cooldown),loaded));CHECK(loaded.cooldownRemainingMs==600);CHECK(loaded.pendingCasts.empty());
    CHECK(!Cast(loaded,DefaultSpell(loaded.affinity),0).ok);CHECK(Tick(loaded,600).ok);CHECK(Cast(loaded,DefaultSpell(loaded.affinity),loaded.serverTimeMs).ok);
}
}
int main() {
    IdentityAndAwakening();StoryOrderAndEveryAffinity();BoundedSpellsAndClock();CastProofAndGuardianPhases();
    MasteryAndFusion();ExperimentsAndEconomy();HospitalAndInnServices();SavePersistenceAndMigration();
    std::cout<<"Aetheria native rules: "<<checks<<" assertions, "<<failures<<" failures\n";
    return failures?EXIT_FAILURE:EXIT_SUCCESS;
}
