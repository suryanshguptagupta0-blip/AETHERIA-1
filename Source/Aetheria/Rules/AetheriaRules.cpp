#include "AetheriaRules.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <set>

namespace Aetheria { namespace Rules {
namespace {
constexpr std::int64_t MaxCoins = 1000000000000LL;
constexpr std::uint32_t MaxMasteryXp = 1000000;
constexpr std::uint32_t MaxCredits = 1000;
constexpr std::uint8_t AllFacilities = 0x7f;
constexpr std::size_t MaxPendingCasts = 32;
constexpr std::array<std::uint32_t, StageCount> Thresholds{{0,40,100,180,300,460}};
bool ValidElement(Element e) { return static_cast<unsigned>(e) < ElementCount; }
bool ValidChapter(Chapter c) { return static_cast<unsigned>(c) <= static_cast<unsigned>(Chapter::Complete); }
bool ValidStage(Stage s) { return static_cast<unsigned>(s) < StageCount; }
std::size_t Index(Element e) { return static_cast<std::size_t>(e); }
bool HasAffinity(const State& s) { return ValidElement(s.affinity) && s.mastery[Index(s.affinity)].awakened; }
Result Fail(const State& s, Error e, const char* message) { Result r; r.error=e; r.message=message; r.chapter=s.chapter; return r; }
Result Ok(State& s, const char* message) { Result r; r.ok=true; r.message=message; r.chapter=s.chapter; ++s.eventSequence; return r; }
bool SetError(std::string* e, const char* message) { if(e) *e=message; return false; }
bool GoodText(const std::string& text, std::size_t max, bool empty=false) {
    if((!empty && text.empty()) || text.size()>max) return false;
    for(unsigned char c:text) if(c<32 || c==127) return false;
    return true;
}
bool EconomyOpen(const State& s) { return HasAffinity(s) && s.chapter>=Chapter::CityVisits; }
void AddXp(State& s, std::uint32_t xp) {
    s.xp=std::min<std::uint32_t>(1000000,s.xp+xp);
    while(s.level<100 && s.xp>=s.level*150) { s.xp-=s.level*150; ++s.level; }
}
void AddMastery(State& s, Element e, std::uint32_t xp) {
    auto& m=s.mastery[Index(e)];
    m.xp=std::min(MaxMasteryXp,m.xp+xp);
    m.stage=StageForXp(m.xp);
}
void FinishTrial(State& s) {
    if(s.trialHits>=3 && s.trialDodges>=1 && s.trialInteractions>=1) {
        s.chapter=Chapter::GuardianEncounter; AddXp(s,30);
    }
}
Stage EffectiveStage(const State& s, const SpellSpec& spec) {
    if(spec.fusionIndex>=0 && static_cast<std::size_t>(spec.fusionIndex)<s.fusions.size()) return s.fusions[spec.fusionIndex].stage;
    if(ValidElement(spec.element)) return s.mastery[Index(spec.element)].stage;
    return Stage::Basic;
}
bool SameSpec(const SpellSpec& a,const SpellSpec& b) {
    return a.element==b.element && a.form==b.form && a.shape==b.shape &&
        a.delivery==b.delivery && a.behavior==b.behavior && a.range==b.range &&
        a.speed==b.speed && a.intensity==b.intensity && a.duration==b.duration &&
        a.target==b.target && a.secondary==b.secondary && a.fusionIndex==b.fusionIndex;
}
const char* FusionName(Element a, Element b) {
    auto is=[&](Element x,Element y){return (a==x&&b==y)||(a==y&&b==x);};
    if(is(Element::Fire,Element::Earth)) return "Magma";
    if(is(Element::Water,Element::Air)) return "Cryotempest";
    if(is(Element::Light,Element::Healing)) return "Radiant Vitality";
    if(is(Element::Space,Element::Dark)) return "Void";
    if(is(Element::Summon,Element::Shadow)) return "Umbral Companion";
    if(is(Element::Electric,Element::Water)) return "Conductive Tide";
    return nullptr;
}
int ItemPrice(const std::string& id) {
    if(id=="mana-potion") return static_cast<int>(ManaPotionGoldCost);
    if(id=="health-potion") return static_cast<int>(HealthPotionGoldCost);
    return 0;
}
void AddInventory(State& s,const std::string& id,int count) {
    for(auto& entry:s.inventory) if(entry.id==id) { entry.count+=count; return; }
    s.inventory.push_back({id,count});
}
int InventoryCount(const State& s,const std::string& id) {
    for(const auto& entry:s.inventory) if(entry.id==id) return entry.count;
    return 0;
}
void AdvanceClock(State& s,std::uint64_t now) {
    const std::uint64_t elapsed=now-s.serverTimeMs;
    s.cooldownRemainingMs=elapsed>=s.cooldownRemainingMs?0:s.cooldownRemainingMs-static_cast<std::uint32_t>(elapsed);
    s.serverTimeMs=now;
    s.pendingCasts.erase(std::remove_if(s.pendingCasts.begin(),s.pendingCasts.end(),
        [&](const PendingCast& p){return p.resolved || p.expiresAtMs<now;}),s.pendingCasts.end());
}
bool NumberIn(float value,float min,float max) { return std::isfinite(value) && value>=min && value<=max; }
bool ValidSpecShape(const SpellSpec& p) {
    return ValidElement(p.element) && static_cast<unsigned>(p.form)<=static_cast<unsigned>(Form::Restore) &&
        static_cast<unsigned>(p.shape)<=static_cast<unsigned>(Shape::Humanoid) &&
        static_cast<unsigned>(p.delivery)<=static_cast<unsigned>(Delivery::Self) &&
        static_cast<unsigned>(p.behavior)<=static_cast<unsigned>(Behavior::Heal) &&
        static_cast<unsigned>(p.target)<=static_cast<unsigned>(Target::Ground) &&
        static_cast<unsigned>(p.secondary)<=static_cast<unsigned>(SecondaryEffect::Weaken) &&
        NumberIn(p.range,0,72) && NumberIn(p.speed,3,60) && NumberIn(p.intensity,0.5f,5) &&
        NumberIn(p.duration,0,10) && p.fusionIndex>=-1;
}
bool IsBasicCompatible(const SpellSpec& p,Stage stage) {
    if(p.form==Form::Displacement && (p.element!=Element::Space || stage<Stage::High)) return false;
    if(p.form==Form::Construct && p.element!=Element::Summon) return false;
    if(p.form==Form::Restore && p.element!=Element::Healing) return false;
    if((p.form==Form::Stream || p.form==Form::Shield) && stage<Stage::Real) return false;
    if(p.form==Form::Wall && stage<Stage::High) return false;
    if(p.secondary!=SecondaryEffect::None && stage<Stage::Real) return false;
    if(p.form==Form::Restore && (p.target!=Target::Self && p.target!=Target::Ally)) return false;
    if(p.form==Form::Displacement && p.target!=Target::Self) return false;
    if(p.form==Form::Projectile && (p.delivery!=Delivery::Bolt || p.behavior!=Behavior::Impact)) return false;
    if(p.form==Form::Restore && p.behavior!=Behavior::Heal) return false;
    if(p.form==Form::Displacement && p.behavior!=Behavior::Move) return false;
    if(p.form==Form::Construct && p.behavior!=Behavior::Follow) return false;
    if((p.form==Form::Shield || p.form==Form::Wall) && p.behavior!=Behavior::Protect) return false;
    if(p.form==Form::Stream && p.behavior!=Behavior::Sustain) return false;
    return true;
}
struct Writer {
    std::vector<std::uint8_t> bytes;
    void U8(std::uint8_t v){bytes.push_back(v);}
    void U32(std::uint32_t v){for(int i=0;i<4;++i)U8(static_cast<std::uint8_t>(v>>(i*8)));}
    void U64(std::uint64_t v){for(int i=0;i<8;++i)U8(static_cast<std::uint8_t>(v>>(i*8)));}
    void I32(std::int32_t v){U32(static_cast<std::uint32_t>(v));}
    void I64(std::int64_t v){U64(static_cast<std::uint64_t>(v));}
    void Float(float v){std::uint32_t bits=0;std::memcpy(&bits,&v,4);U32(bits);}
    void Text(const std::string& v){U32(static_cast<std::uint32_t>(v.size()));bytes.insert(bytes.end(),v.begin(),v.end());}
};
struct Reader {
    const std::vector<std::uint8_t>& bytes; std::size_t offset=0,limit=0; bool good=true;
    std::uint8_t U8(){if(offset>=limit){good=false;return 0;}return bytes[offset++];}
    std::uint32_t U32(){std::uint32_t v=0;for(int i=0;i<4;++i)v|=static_cast<std::uint32_t>(U8())<<(i*8);return v;}
    std::uint64_t U64(){std::uint64_t v=0;for(int i=0;i<8;++i)v|=static_cast<std::uint64_t>(U8())<<(i*8);return v;}
    std::int32_t I32(){return static_cast<std::int32_t>(U32());}
    std::int64_t I64(){return static_cast<std::int64_t>(U64());}
    float Float(){std::uint32_t bits=U32();float v=0;std::memcpy(&v,&bits,4);return v;}
    std::string Text(std::size_t max){auto n=U32();if(!good||n>max||n>limit-offset){good=false;return {};}std::string s(bytes.begin()+offset,bytes.begin()+offset+n);offset+=n;return s;}
};
std::uint32_t Checksum(const std::vector<std::uint8_t>& data,std::size_t size) {
    std::uint32_t h=2166136261u;for(std::size_t i=0;i<size;++i){h^=data[i];h*=16777619u;}return h;
}
void WriteSpec(Writer& w,const SpellSpec& p) {
    w.U8(static_cast<std::uint8_t>(p.element));w.U8(static_cast<std::uint8_t>(p.form));
    w.U8(static_cast<std::uint8_t>(p.shape));w.U8(static_cast<std::uint8_t>(p.delivery));
    w.U8(static_cast<std::uint8_t>(p.behavior));w.Float(p.range);w.Float(p.speed);
    w.Float(p.intensity);w.Float(p.duration);w.U8(static_cast<std::uint8_t>(p.target));
    w.U8(static_cast<std::uint8_t>(p.secondary));w.I32(p.fusionIndex);
}
SpellSpec ReadSpec(Reader& r) {
    SpellSpec p;p.element=static_cast<Element>(r.U8());p.form=static_cast<Form>(r.U8());
    p.shape=static_cast<Shape>(r.U8());p.delivery=static_cast<Delivery>(r.U8());
    p.behavior=static_cast<Behavior>(r.U8());p.range=r.Float();p.speed=r.Float();
    p.intensity=r.Float();p.duration=r.Float();p.target=static_cast<Target>(r.U8());
    p.secondary=static_cast<SecondaryEffect>(r.U8());p.fusionIndex=r.I32();return p;
}
} // namespace

const char* ElementName(Element e) {
    static constexpr const char* names[]{"Fire","Water","Air","Earth","Space","Shadow","Dark","Light","Thunder / Electricity","Summoning","Healing / Vitality"};
    return ValidElement(e)?names[Index(e)]:"Unattuned";
}
const char* StageName(Stage s) {
    static constexpr const char* names[]{"BASIC","TRUE / REAL","HIGH","ADVANCED","SOVEREIGN / SUPER","PERFECT"};
    return ValidStage(s)?names[static_cast<unsigned>(s)]:"INVALID";
}
Stage StageForXp(std::uint32_t xp) {
    std::size_t stage=0;for(std::size_t i=1;i<StageCount;++i)if(xp>=Thresholds[i])stage=i;
    return static_cast<Stage>(stage);
}
State CreateNewPlayer(const std::string& name) {
    State s;s.health=70;s.concentration=75;if(GoodText(name,32))s.name=name;return s;
}
SpellSpec DefaultSpell(Element e) {
    SpellSpec p;p.element=e;
    if(e==Element::Summon){p.form=Form::Construct;p.shape=Shape::Humanoid;p.delivery=Delivery::Placed;p.behavior=Behavior::Follow;p.duration=2;}
    // Vitality pulses damage a fracture construct; Restore is a separate healing-only form.
    return p;
}
std::string GetObjective(const State& s) {
    switch(s.chapter) {
    case Chapter::HospitalStart:return "Speak with the hospital doctor about the mysterious fracture.";
    case Chapter::AwakeningHall:return s.mentorMet?"Approach the crystal and let Elder Seraphine begin your First Awakening.":"Find Elder Seraphine inside the connected Awakening Hall.";
    case Chapter::FirstMagicTrial:return "Use your own affinity: hit three practice targets, dodge a telegraph and complete an elemental interaction.";
    case Chapter::GuardianEncounter:return "Defeat the Fracture Guardian through both phases. Watch its telegraphs and preserve mana.";
    case Chapter::CityVisits:return "Visit the Hospital, Academy, Guild Hall, Houses, Market, Bank and Inn.";
    case Chapter::FinalDecision:return "Return to Elder Seraphine. Seal the fracture or preserve a controlled fragment for study.";
    case Chapter::Complete:return s.finalChoice==FinalChoice::Seal?"First Awakening complete: the fracture is sealed.":"First Awakening complete: the controlled fragment is preserved for study.";
    }
    return "Invalid story state.";
}
Result ApplyStoryEvent(State& s,StoryEvent event) {
    if(static_cast<unsigned>(event)>static_cast<unsigned>(StoryEvent::ReturnToSeraphine))return Fail(s,Error::InvalidInput,"Unknown story event.");
    if(event==StoryEvent::MeetDoctor) {
        if(s.chapter!=Chapter::HospitalStart)return Fail(s,Error::WrongChapter,"The hospital introduction is already complete.");
        s.health=100;s.concentration=100;s.chapter=Chapter::AwakeningHall;return Ok(s,"The doctor treats your injuries and directs you to Elder Seraphine.");
    }
    if(event==StoryEvent::MeetSeraphine) {
        if(s.chapter!=Chapter::AwakeningHall)return Fail(s,Error::WrongChapter,"Seraphine awaits the hospital introduction.");
        if(s.mentorMet)return Fail(s,Error::AlreadyCompleted,"You have already met Seraphine.");
        s.mentorMet=true;return Ok(s,"Seraphine asks you to approach the awakening crystal.");
    }
    if(event==StoryEvent::TrialTargetHit || event==StoryEvent::TrialDodge || event==StoryEvent::TrialElementInteraction) {
        if(s.chapter!=Chapter::FirstMagicTrial || !HasAffinity(s))return Fail(s,Error::WrongChapter,"The First Magic Trial is not active.");
        auto& count=event==StoryEvent::TrialTargetHit?s.trialHits:event==StoryEvent::TrialDodge?s.trialDodges:s.trialInteractions;
        const std::uint32_t limit=event==StoryEvent::TrialTargetHit?3:1;
        if(count>=limit)return Fail(s,Error::AlreadyCompleted,"This trial requirement is already complete.");
        ++count;
        if(event==StoryEvent::TrialTargetHit) {
            auto& m=s.mastery[Index(s.affinity)];m.trainingCredits=std::min(MaxCredits,m.trainingCredits+1);
        }
        FinishTrial(s);return Ok(s,"A server-verified trial action has been recorded.");
    }
    if(event==StoryEvent::GuardianDefeated) {
        if(!s.guardianDefeated)return Fail(s,Error::WrongChapter,"The authoritative Guardian encounter is not defeated.");
        return Fail(s,Error::AlreadyCompleted,"Guardian completion has already been recorded.");
    }
    if(event>=StoryEvent::VisitHospital && event<=StoryEvent::VisitInn) {
        if(s.chapter!=Chapter::CityVisits)return Fail(s,Error::WrongChapter,"Complete the Guardian encounter before the city introduction.");
        const auto facility=static_cast<unsigned>(event)-static_cast<unsigned>(StoryEvent::VisitHospital);
        const auto bit=static_cast<std::uint8_t>(1u<<facility);
        if(s.visitedFacilities&bit)return Fail(s,Error::AlreadyCompleted,"This facility visit is already recorded.");
        s.visitedFacilities|=bit;
        if(s.visitedFacilities==AllFacilities)s.chapter=Chapter::FinalDecision;
        return Ok(s,"The connected city visit has been recorded.");
    }
    if(event==StoryEvent::ReturnToSeraphine) {
        if(s.chapter!=Chapter::FinalDecision)return Fail(s,Error::WrongChapter,"Complete all seven city visits first.");
        if(s.mentorMet)return Fail(s,Error::AlreadyCompleted,"Seraphine is ready for your final decision.");
        s.mentorMet=true;return Ok(s,"Seraphine offers the final awakening decision.");
    }
    return Fail(s,Error::InvalidInput,"Unknown story event.");
}
Result Awaken(State& s,std::uint32_t random) {
    if(s.chapter!=Chapter::AwakeningHall || !s.mentorMet)return Fail(s,Error::WrongChapter,"Meet Seraphine at the awakening crystal first.");
    if(HasAffinity(s))return Fail(s,Error::AlreadyCompleted,"A permanent primary affinity has already been assigned.");
    for(const auto& m:s.mastery)if(m.awakened)return Fail(s,Error::InvalidInput,"An unattuned player cannot already own elements.");
    // Multiply-high distributes all eleven outcomes over the complete server RNG domain.
    const auto index=static_cast<std::size_t>((static_cast<std::uint64_t>(random)*ElementCount)>>32);
    s.affinity=static_cast<Element>(index);auto& m=s.mastery[index];
    m.awakened=true;m.xp=0;m.stage=Stage::Basic;m.awakenedAtLevel=1;
    s.chapter=Chapter::FirstMagicTrial;s.mentorMet=false;s.mana=100;
    Result r=Ok(s,"Your primary affinity is permanently awakened at BASIC.");r.element=s.affinity;return r;
}
Result ChooseFinal(State& s,FinalChoice choice) {
    if(choice!=FinalChoice::Seal && choice!=FinalChoice::Study)return Fail(s,Error::InvalidInput,"Choose Seal or Study.");
    if(s.chapter!=Chapter::FinalDecision || !s.mentorMet)return Fail(s,Error::WrongChapter,"Return to Seraphine after all city visits.");
    if(s.finalChoice!=FinalChoice::None)return Fail(s,Error::AlreadyCompleted,"Your final decision is permanent.");
    if(s.rewardTreasury<100 || s.wallet>MaxCoins-100)return Fail(s,Error::InsufficientFunds,"The reward account cannot complete the transaction.");
    s.finalChoice=choice;s.chapter=Chapter::Complete;s.wallet+=100;s.rewardTreasury-=100;AddXp(s,100);
    return Ok(s,choice==FinalChoice::Seal?"The fracture is sealed. First Awakening is complete.":"A controlled fragment is preserved. First Awakening is complete.");
}
Result Train(State& s,Element e) {
    if(!ValidElement(e))return Fail(s,Error::InvalidInput,"Unknown primary element.");
    auto& m=s.mastery[Index(e)];
    if(!HasAffinity(s) || !m.awakened)return Fail(s,Error::WrongAffinity,"Train an awakened affinity.");
    if(m.stage==Stage::Perfect)return Fail(s,Error::InsufficientMastery,"This element is already PERFECT.");
    if(!m.trainingCredits)return Fail(s,Error::MissingTraining,"Complete a real target drill or combat hit before training.");
    if(s.wallet<TrainingGoldCost || s.merchantTreasury>MaxCoins-TrainingGoldCost)return Fail(s,Error::InsufficientFunds,"Training costs 20 gold.");
    if(s.mana<TrainingManaCost)return Fail(s,Error::InsufficientMana,"Training costs 8 mana.");
    --m.trainingCredits;s.wallet-=TrainingGoldCost;s.merchantTreasury+=TrainingGoldCost;s.mana-=TrainingManaCost;AddMastery(s,e,40);AddXp(s,20);
    return Ok(s,"A completed drill has advanced mastery.");
}
std::int32_t GetSpellCost(const State& s,const SpellSpec& p) {
    if(!ValidSpecShape(p))return -1;
    const int stage=static_cast<int>(EffectiveStage(s,p));
    const float formCost=p.form==Form::Construct?6.0f:p.form==Form::Displacement?8.0f:p.form==Form::Restore?5.0f:0.0f;
    float base=5.0f+p.intensity*4.0f+p.range*0.12f+p.duration*2.0f+formCost+(p.secondary!=SecondaryEffect::None?3.0f:0.0f);
    if(p.fusionIndex>=0)base*=1.6f;
    return std::max(3,static_cast<int>(std::ceil(base*(1.0f-stage*0.06f))));
}
Result ValidateSpell(const State& s,const SpellSpec& p) {
    if(!ValidSpecShape(p))return Fail(s,Error::InvalidInput,"Spell parameters must be finite, bounded and valid.");
    if(!HasAffinity(s))return Fail(s,Error::Unattuned,"Complete First Awakening before casting.");
    if(p.fusionIndex>=0) {
        if(static_cast<std::size_t>(p.fusionIndex)>=s.fusions.size())return Fail(s,Error::WrongAffinity,"This fusion has not been unlocked.");
        const auto& f=s.fusions[p.fusionIndex];
        if(p.element!=f.first && p.element!=f.second)return Fail(s,Error::WrongAffinity,"The spell does not match its fusion.");
        if(s.mastery[Index(f.first)].stage<Stage::Advanced || s.mastery[Index(f.second)].stage<Stage::Advanced)return Fail(s,Error::InsufficientMastery,"Both fusion parents must remain ADVANCED.");
    } else if(!s.mastery[Index(p.element)].awakened) return Fail(s,Error::WrongAffinity,"The requested element is not awakened.");
    const auto stage=EffectiveStage(s,p);const int n=static_cast<int>(stage);
    if(p.range>18.0f+n*10.0f || p.speed>30.0f+n*6.0f || p.intensity>2.0f+n*0.6f || p.duration>2.0f+n*1.6f || !IsBasicCompatible(p,stage))
        return Fail(s,Error::InsufficientMastery,"This spell exceeds your mastery controls.");
    if(s.health<=0 || s.concentration<30 || s.fatigue>=80 || (s.health<20 && p.form!=Form::Restore))return Fail(s,Error::Unstable,"Recover concentration, fatigue or injury before casting.");
    Result r;r.ok=true;r.chapter=s.chapter;r.element=p.element;r.manaCost=GetSpellCost(s,p);return r;
}
Result Cast(State& s,const SpellSpec& p,std::uint64_t now) {
    Result validation=ValidateSpell(s,p);if(!validation.ok)return validation;
    if(now<s.serverTimeMs)return Fail(s,Error::InvalidInput,"Server casting time must not move backwards.");
    State next=s;AdvanceClock(next,now);
    if(next.cooldownRemainingMs)return Fail(s,Error::Cooldown,"The spell is still recovering.");
    if(next.mana<validation.manaCost)return Fail(s,Error::InsufficientMana,"Not enough mana.");
    if(next.pendingCasts.size()>=MaxPendingCasts)return Fail(s,Error::Capacity,"Too many unresolved spell effects.");
    if(now>std::numeric_limits<std::uint64_t>::max()-10000 || next.eventSequence==std::numeric_limits<std::uint64_t>::max())
        return Fail(s,Error::InvalidInput,"Server timestamp or event sequence overflow.");
    next.mana-=validation.manaCost;next.fatigue=std::min(100,next.fatigue+3);
    next.cooldownRemainingMs=600+static_cast<std::uint32_t>(p.duration*100);next.lastCastAtMs=now;
    Result r=Ok(next,"The server accepted the bounded spell.");r.element=p.element;r.manaCost=validation.manaCost;
    const int stage=static_cast<int>(EffectiveStage(next,p));
    r.damage=p.form==Form::Restore || p.form==Form::Shield || p.form==Form::Wall || p.form==Form::Displacement?0:
        std::min(100,static_cast<int>(std::lround((12.0f+stage*4.0f)*p.intensity)));
    r.healing=p.form==Form::Restore?std::min(45,static_cast<int>(std::lround(15.0f*p.intensity))):0;
    if(p.form==Form::Restore && p.target==Target::Self && next.health>0)next.health=std::min(100,next.health+r.healing);
    r.castId=next.eventSequence;next.pendingCasts.push_back({r.castId,now+10000,r.damage,p.element,false});
    if(p.fusionIndex>=0){auto& f=next.fusions[p.fusionIndex];f.xp=std::min(MaxMasteryXp,f.xp+2);f.stage=StageForXp(f.xp);}
    else AddMastery(next,p.element,2);
    s=std::move(next);return r;
}
Result ExperimentSpell(State& s,const SpellSpec& p,std::uint64_t now) {
    if(s.spells.size()>=MaxSpells)return Fail(s,Error::Capacity,"The spell journal is full.");
    for(const auto& spell:s.spells)if(SameSpec(spell.spec,p))return Fail(s,Error::Duplicate,"This experiment is already recorded.");
    State next=s;Result cast=Cast(next,p,now);if(!cast.ok)return cast;
    std::uint32_t id=1;for(const auto& spell:next.spells)id=std::max(id,spell.id+1);
    next.spells.push_back({id,"Unnamed experiment",p,SpellPhase::Experiment,0});s=std::move(next);
    cast.message="Experiment recorded. Stabilize it before reproduction.";return cast;
}
Result StabilizeSpell(State& s,std::uint32_t id) {
    auto it=std::find_if(s.spells.begin(),s.spells.end(),[&](const NamedSpell& n){return n.id==id;});
    if(it==s.spells.end())return Fail(s,Error::InvalidInput,"Unknown spell experiment.");
    if(it->phase!=SpellPhase::Experiment)return Fail(s,Error::WrongChapter,"Only an experiment can be stabilized.");
    auto valid=ValidateSpell(s,it->spec);if(!valid.ok)return valid;
    if(s.mana<10)return Fail(s,Error::InsufficientMana,"Stabilization costs 10 mana.");
    s.mana-=10;it->phase=SpellPhase::Stabilized;return Ok(s,"The experiment is stabilized. Reproduce it twice.");
}
Result ReproduceSpell(State& s,std::uint32_t id,std::uint64_t now) {
    auto it=std::find_if(s.spells.begin(),s.spells.end(),[&](const NamedSpell& n){return n.id==id;});
    if(it==s.spells.end())return Fail(s,Error::InvalidInput,"Unknown spell experiment.");
    if(it->phase!=SpellPhase::Stabilized)return Fail(s,Error::WrongChapter,"Stabilize the experiment before reproduction.");
    const auto index=static_cast<std::size_t>(it-s.spells.begin());State next=s;
    Result result=Cast(next,it->spec,now);if(!result.ok)return result;
    auto& spell=next.spells[index];++spell.reproductions;if(spell.reproductions>=2)spell.phase=SpellPhase::Reproduced;
    result.message="A successful reproduction is recorded.";s=std::move(next);return result;
}
Result RegisterSpell(State& s,const SpellSpec& p,const std::string& name) {
    if(!GoodText(name,48))return Fail(s,Error::InvalidInput,"Spell names must contain 1 to 48 printable bytes.");
    auto valid=ValidateSpell(s,p);if(!valid.ok)return valid;
    auto it=std::find_if(s.spells.begin(),s.spells.end(),[&](const NamedSpell& n){return SameSpec(n.spec,p);});
    if(it==s.spells.end() || it->phase!=SpellPhase::Reproduced)return Fail(s,Error::WrongChapter,"Experiment, stabilize and reproduce this spell before registration.");
    for(const auto& spell:s.spells)if(spell.phase==SpellPhase::Registered && spell.name==name)return Fail(s,Error::Duplicate,"This registered name is already in use.");
    it->name=name;it->phase=SpellPhase::Registered;return Ok(s,"The named spell is registered permanently.");
}
Result Fuse(State& s,Element a,Element b) {
    if(!ValidElement(a)||!ValidElement(b)||a==b)return Fail(s,Error::InvalidInput,"Fusion needs two distinct primary elements.");
    if(!HasAffinity(s)||!s.mastery[Index(a)].awakened||!s.mastery[Index(b)].awakened)return Fail(s,Error::WrongAffinity,"Both fusion parents must be awakened.");
    if(s.mastery[Index(a)].stage<Stage::Advanced||s.mastery[Index(b)].stage<Stage::Advanced)return Fail(s,Error::InsufficientMastery,"Both parents must be ADVANCED.");
    const char* name=FusionName(a,b);if(!name)return Fail(s,Error::InvalidInput,"These elements have no stable authored fusion.");
    for(const auto& f:s.fusions)if((f.first==a&&f.second==b)||(f.first==b&&f.second==a))return Fail(s,Error::Duplicate,"This fusion is already awakened.");
    if(s.fusions.size()>=MaxFusions)return Fail(s,Error::Capacity,"The fusion journal is full.");
    if(s.mana<35)return Fail(s,Error::InsufficientMana,"Fusion requires 35 mana.");
    s.mana-=35;s.fusions.push_back({a,b,name,Stage::Basic,0});return Ok(s,"A stable fusion has awakened at BASIC.");
}
Result Deposit(State& s,std::int64_t amount) {
    if(!EconomyOpen(s))return Fail(s,Error::WrongChapter,"Visit the bank after the Guardian encounter.");
    if(amount<=0 || amount>MaxCoins)return Fail(s,Error::InvalidInput,"Deposit a positive bounded amount.");
    if(s.wallet<amount || s.bank>MaxCoins-amount)return Fail(s,Error::InsufficientFunds,"The deposit cannot be funded.");
    s.wallet-=amount;s.bank+=amount;return Ok(s,"Bank deposit recorded.");
}
Result Withdraw(State& s,std::int64_t amount) {
    if(!EconomyOpen(s))return Fail(s,Error::WrongChapter,"Visit the bank after the Guardian encounter.");
    if(amount<=0 || amount>MaxCoins)return Fail(s,Error::InvalidInput,"Withdraw a positive bounded amount.");
    if(s.bank<amount || s.wallet>MaxCoins-amount)return Fail(s,Error::InsufficientFunds,"The withdrawal cannot be funded.");
    s.bank-=amount;s.wallet+=amount;return Ok(s,"Bank withdrawal recorded.");
}
Result BuyItem(State& s,const std::string& id,std::int32_t quantity) {
    if(!EconomyOpen(s))return Fail(s,Error::WrongChapter,"The market opens during the city introduction.");
    const int price=ItemPrice(id);
    if(!price || quantity<=0 || quantity>99)return Fail(s,Error::InvalidInput,"Choose a known item and quantity from 1 to 99.");
    const std::int64_t cost=static_cast<std::int64_t>(price)*quantity;
    if(s.wallet<cost || s.merchantTreasury>MaxCoins-cost)return Fail(s,Error::InsufficientFunds,"The purchase cannot be funded.");
    if(InventoryCount(s,id)>99-quantity)return Fail(s,Error::Capacity,"The item stack is full.");
    s.wallet-=cost;s.merchantTreasury+=cost;AddInventory(s,id,quantity);return Ok(s,"The merchant transaction is recorded.");
}
Result UseItem(State& s,const std::string& id) {
    if(!HasAffinity(s))return Fail(s,Error::Unattuned,"Complete the awakening ceremony first.");
    if(!ItemPrice(id))return Fail(s,Error::InvalidInput,"Unknown consumable.");
    auto it=std::find_if(s.inventory.begin(),s.inventory.end(),[&](const InventoryEntry& e){return e.id==id;});
    if(it==s.inventory.end() || it->count<=0)return Fail(s,Error::InvalidInput,"This item is not in your inventory.");
    if(id=="health-potion" && s.health==0)return Fail(s,Error::Unstable,"A potion cannot resurrect a dead character.");
    if((id=="mana-potion"&&s.mana==100)||(id=="health-potion"&&s.health==100))return Fail(s,Error::InvalidInput,"This resource is already full.");
    --it->count;if(id=="mana-potion")s.mana=std::min(100,s.mana+40);else s.health=std::min(100,s.health+40);
    if(it->count==0) s.inventory.erase(it);
    return Ok(s,"The consumable has been used.");
}
Result Tick(State& s,std::uint32_t elapsed) {
    if(elapsed==0 || elapsed>60000 || s.serverTimeMs>std::numeric_limits<std::uint64_t>::max()-elapsed)
        return Fail(s,Error::InvalidInput,"Tick requires a bounded positive server interval.");
    State next=s;AdvanceClock(next,next.serverTimeMs+elapsed);
    const std::uint32_t total=next.regenRemainderMs+elapsed,seconds=total/1000;next.regenRemainderMs=total%1000;
    if(next.health>0) {
        next.mana=std::min(100,next.mana+static_cast<int>(seconds*5));
        next.stamina=std::min(100,next.stamina+static_cast<int>(seconds*8));
        next.fatigue=std::max(0,next.fatigue-static_cast<int>(seconds*2));
        next.concentration=std::min(100,next.concentration+static_cast<int>(seconds*2));
    }
    s=std::move(next);Result r;r.ok=true;r.chapter=s.chapter;r.message="Authoritative resources advanced.";return r;
}
Result ApplyDamage(State& s,std::int32_t damage) {
    if(damage<=0 || damage>100)return Fail(s,Error::InvalidInput,"Damage must be a bounded server-resolved effect.");
    if(s.health<=0)return Fail(s,Error::AlreadyCompleted,"The character is already incapacitated.");
    s.health=std::max(0,s.health-damage);s.concentration=std::max(0,s.concentration-damage/2);
    return Ok(s,s.health?"A server-validated attack hit the character.":"The character is incapacitated; healing is not resurrection.");
}
GuardianState CreateGuardian(){return GuardianState{};}
Result ResolveGuardianHit(State& s,GuardianState& g,std::uint64_t id,std::uint64_t now) {
    if(s.chapter!=Chapter::GuardianEncounter || s.guardianDefeated || g.phase==GuardianPhase::Defeated)
        return Fail(s,Error::WrongChapter,"The Guardian encounter is not active.");
    if(g.maxHealth!=240 || g.health<=0 || g.health>g.maxHealth || static_cast<unsigned>(g.phase)>2 ||
        (g.phase==GuardianPhase::One&&(g.health<=120||g.phaseTwoReached)) ||
        (g.phase==GuardianPhase::Two&&(g.health>120||!g.phaseTwoReached)))
        return Fail(s,Error::InvalidInput,"Invalid authoritative Guardian state.");
    if(now<s.serverTimeMs)return Fail(s,Error::InvalidInput,"Server hit time must not move backwards.");
    auto it=std::find_if(s.pendingCasts.begin(),s.pendingCasts.end(),[&](const PendingCast& p){return p.id==id;});
    if(it==s.pendingCasts.end() || it->resolved || it->expiresAtMs<now)return Fail(s,Error::ExpiredHit,"The server cast token is absent, expired or already resolved.");
    if(it->damage<=0)return Fail(s,Error::InvalidInput,"This spell does not damage the Guardian.");
    if(s.rewardTreasury<50 || s.wallet>MaxCoins-50)return Fail(s,Error::InsufficientFunds,"Guardian reward funds are unavailable.");
    State next=s;GuardianState boss=g;
    const int damage=it->damage;const Element hitElement=it->element;
    AdvanceClock(next,now);
    auto token=std::find_if(next.pendingCasts.begin(),next.pendingCasts.end(),[&](const PendingCast& p){return p.id==id;});
    token->resolved=true;
    boss.health=std::max(0,boss.health-damage);
    if(boss.phase==GuardianPhase::One && boss.health<=boss.maxHealth/2) {
        boss.health=boss.maxHealth/2;boss.phase=GuardianPhase::Two;boss.phaseTwoReached=true;
    } else if(boss.phase==GuardianPhase::Two && boss.health==0) {
        if(!boss.phaseTwoReached)return Fail(s,Error::InvalidInput,"Phase two must be entered before defeat.");
        boss.phase=GuardianPhase::Defeated;next.guardianDefeated=true;next.chapter=Chapter::CityVisits;
        next.mentorMet=false;next.wallet+=50;next.rewardTreasury-=50;AddXp(next,220);
    }
    auto& mastery=next.mastery[Index(hitElement)];mastery.trainingCredits=std::min(MaxCredits,mastery.trainingCredits+1);
    Result result=Ok(next,boss.phase==GuardianPhase::Defeated?"The two-phase Guardian is defeated. The city introduction begins.":boss.phase==GuardianPhase::Two?"The Guardian's second phase is active.":"The Guardian takes a server-resolved spell hit.");
    result.damage=g.health-boss.health;result.element=hitElement;s=std::move(next);g=boss;return result;
}

Result SpendStamina(State& s,std::int32_t amount) {
    if(amount<=0 || amount>100)return Fail(s,Error::InvalidInput,"A stamina request must be positive and bounded.");
    if(s.health<=0)return Fail(s,Error::Unstable,"An incapacitated player cannot perform this action.");
    if(s.stamina<amount)return Fail(s,Error::InsufficientMana,"Not enough stamina.");
    s.stamina-=amount;return Ok(s,"The server accepted the stamina action.");
}
Result ConfirmGuardianDefeat(State& s,const GuardianState& g) {
    if(g.phase!=GuardianPhase::Defeated || g.health!=0 || g.maxHealth!=240 || !g.phaseTwoReached)
        return Fail(s,Error::InvalidInput,"Only a verified two-phase Guardian defeat can complete this chapter.");
    if(s.chapter!=Chapter::GuardianEncounter || s.guardianDefeated)
        return Fail(s,Error::AlreadyCompleted,"This Guardian reward is not available.");
    if(s.rewardTreasury<50 || s.wallet>MaxCoins-50)return Fail(s,Error::InsufficientFunds,"The Guardian reward account cannot fund the reward.");
    s.guardianDefeated=true;s.chapter=Chapter::CityVisits;s.mentorMet=false;
    s.wallet+=50;s.rewardTreasury-=50;AddXp(s,220);
    return Ok(s,"The server confirmed your participation in the two-phase Guardian defeat.");
}
bool ValidateState(const State& s,std::string* error) {
    if(s.version!=CurrentSaveVersion)return SetError(error,"Save must use the current schema version.");
    if(!GoodText(s.name,32))return SetError(error,"Invalid character name.");
    const auto& a=s.appearance;
    if((a.sex!="male"&&a.sex!="female") || !GoodText(a.skin,24) || !GoodText(a.hairStyle,24) ||
        !GoodText(a.hairColor,24) || !GoodText(a.eyeColor,24) || !GoodText(a.clothing,32) ||
        !NumberIn(a.bodyScale,0.85f,1.15f))return SetError(error,"Invalid character appearance.");
    if(!ValidChapter(s.chapter) || static_cast<unsigned>(s.finalChoice)>static_cast<unsigned>(FinalChoice::Study))
        return SetError(error,"Invalid story enum.");
    for(int value:{s.health,s.mana,s.stamina,s.concentration,s.fatigue})if(value<0||value>100)return SetError(error,"Resources must be from 0 to 100.");
    if(s.level<1||s.level>100||s.xp>1000000 || (s.level<100&&s.xp>=s.level*150))return SetError(error,"Invalid character progression.");
    for(auto value:{s.wallet,s.bank,s.merchantTreasury,s.rewardTreasury})if(value<0||value>MaxCoins)return SetError(error,"Currency must be nonnegative and bounded.");
    std::set<std::uint32_t> awakeningLevels;std::size_t owned=0;
    for(const auto& m:s.mastery) {
        if(!ValidStage(m.stage)||m.stage!=StageForXp(m.xp)||m.xp>MaxMasteryXp||m.trainingCredits>MaxCredits)
            return SetError(error,"Mastery stage and experience disagree.");
        if(m.awakened) {
            ++owned;
            if(m.awakenedAtLevel<1||m.awakenedAtLevel>s.level||!awakeningLevels.insert(m.awakenedAtLevel).second)
                return SetError(error,"Each awakened element needs a distinct earned character level.");
        } else if(m.xp||m.stage!=Stage::Basic||m.awakenedAtLevel||m.trainingCredits)
            return SetError(error,"Unawakened elements cannot carry progression.");
    }
    if(owned>s.level)return SetError(error,"More elements than earned character levels.");
    if(s.affinity==Element::None) {
        if(owned||s.chapter>Chapter::AwakeningHall)return SetError(error,"An unattuned player cannot claim awakened story progress.");
    } else if(!HasAffinity(s)||s.mastery[Index(s.affinity)].awakenedAtLevel!=1||s.chapter<Chapter::FirstMagicTrial)
        return SetError(error,"Invalid permanent initial affinity.");
    if(s.trialHits>3||s.trialDodges>1||s.trialInteractions>1||s.visitedFacilities>AllFacilities)
        return SetError(error,"Invalid tutorial counters or facility mask.");
    if(s.chapter>=Chapter::GuardianEncounter&&(s.trialHits!=3||s.trialDodges!=1||s.trialInteractions!=1))
        return SetError(error,"Guardian chapter requires the complete trial.");
    if((s.chapter>=Chapter::CityVisits)!=s.guardianDefeated)return SetError(error,"Guardian completion and chapter disagree.");
    if(s.chapter>=Chapter::FinalDecision&&s.visitedFacilities!=AllFacilities)return SetError(error,"Final chapter requires all seven connected visits.");
    if(s.chapter<Chapter::CityVisits&&s.visitedFacilities)return SetError(error,"City visits cannot precede the Guardian.");
    if((s.chapter==Chapter::Complete)!=(s.finalChoice!=FinalChoice::None))return SetError(error,"Final decision and completion disagree.");
    if(s.chapter==Chapter::HospitalStart&&s.mentorMet)return SetError(error,"Seraphine cannot be met before the hospital introduction.");
    if(s.inventory.size()>8||s.spells.size()>MaxSpells||s.fusions.size()>MaxFusions||s.pendingCasts.size()>MaxPendingCasts)
        return SetError(error,"Bounded collection capacity exceeded.");
    std::set<std::string> itemIds;
    for(const auto& item:s.inventory)if(!ItemPrice(item.id)||item.count<1||item.count>99||!itemIds.insert(item.id).second)
        return SetError(error,"Invalid inventory stack.");
    std::set<std::pair<unsigned,unsigned>> pairs;
    for(const auto& f:s.fusions) {
        if(!ValidElement(f.first)||!ValidElement(f.second)||f.first==f.second||!FusionName(f.first,f.second)||
            !GoodText(f.name,48)||!ValidStage(f.stage)||f.stage!=StageForXp(f.xp)||f.xp>MaxMasteryXp)
            return SetError(error,"Invalid fusion definition.");
        if(!s.mastery[Index(f.first)].awakened||!s.mastery[Index(f.second)].awakened||
            s.mastery[Index(f.first)].stage<Stage::Advanced||s.mastery[Index(f.second)].stage<Stage::Advanced)
            return SetError(error,"Fusion parents must both be awakened and ADVANCED.");
        auto a=static_cast<unsigned>(f.first),b=static_cast<unsigned>(f.second);if(a>b)std::swap(a,b);
        if(!pairs.insert({a,b}).second)return SetError(error,"Duplicate fusion parents.");
    }
    std::set<std::uint32_t> spellIds;std::set<std::string> spellNames;
    for(const auto& spell:s.spells) {
        if(spell.id<1||spell.id>1000000||!spellIds.insert(spell.id).second||!GoodText(spell.name,48)||
            static_cast<unsigned>(spell.phase)>static_cast<unsigned>(SpellPhase::Registered)||spell.reproductions>2||
            !ValidSpecShape(spell.spec))return SetError(error,"Invalid named spell.");
        if((spell.phase>=SpellPhase::Reproduced)!=(spell.reproductions==2))return SetError(error,"Spell reproduction phase is inconsistent.");
        if(spell.phase==SpellPhase::Registered&&!spellNames.insert(spell.name).second)return SetError(error,"Duplicate registered spell name.");
        State healthy=s;healthy.health=100;healthy.concentration=100;healthy.fatigue=0;
        if(!ValidateSpell(healthy,spell.spec).ok)return SetError(error,"Saved spell exceeds owned affinity or mastery.");
    }
    if(s.cooldownRemainingMs>1600||s.regenRemainderMs>=1000||s.lastCastAtMs>s.serverTimeMs||
        s.eventSequence==std::numeric_limits<std::uint64_t>::max())return SetError(error,"Invalid saved clock or sequence.");
    std::set<std::uint64_t> castIds;
    for(const auto& p:s.pendingCasts)if(!p.id||p.id>s.eventSequence||!castIds.insert(p.id).second||!ValidElement(p.element)||
        p.damage<0||p.damage>100||p.expiresAtMs<s.serverTimeMs)return SetError(error,"Invalid pending cast.");
    return true;
}
bool MigrateSave(State& state,std::string* error) {
    if(state.version<1||state.version>CurrentSaveVersion)return SetError(error,"Unsupported save schema version.");
    State next=state;
    if(next.version<CurrentSaveVersion) {
        std::uint32_t level=2;
        if(next.affinity==Element::None) {
            for(std::size_t i=0;i<ElementCount;++i)if(next.mastery[i].awakened){next.affinity=static_cast<Element>(i);break;}
        }
        for(std::size_t i=0;i<ElementCount;++i) {
            auto& m=next.mastery[i];m.stage=StageForXp(m.xp);
            if(m.awakened&&m.awakenedAtLevel==0)m.awakenedAtLevel=static_cast<Element>(i)==next.affinity?1:level++;
        }
        for(auto& f:next.fusions)f.stage=StageForXp(f.xp);
        // Versions 1 and 2 stored only registered spells; no experimental pipeline existed.
        for(auto& spell:next.spells){spell.phase=SpellPhase::Registered;spell.reproductions=2;}
        if(HasAffinity(next)&&next.chapter<Chapter::FirstMagicTrial)next.chapter=Chapter::FirstMagicTrial;
    }
    next.version=CurrentSaveVersion;
    next.pendingCasts.clear();next.serverTimeMs=0;next.lastCastAtMs=0;
    if(!ValidateState(next,error))return false;
    state=std::move(next);return true;
}
std::vector<std::uint8_t> EncodeSave(const State& s) {
    if(!ValidateState(s))return {};
    Writer w;for(auto c:std::string("AFRS"))w.U8(static_cast<std::uint8_t>(c));w.U32(CurrentSaveVersion);
    w.Text(s.name);w.Text(s.appearance.sex);w.Text(s.appearance.skin);w.Text(s.appearance.hairStyle);
    w.Text(s.appearance.hairColor);w.Text(s.appearance.eyeColor);w.Text(s.appearance.clothing);w.Float(s.appearance.bodyScale);
    w.U8(static_cast<std::uint8_t>(s.affinity));w.U8(static_cast<std::uint8_t>(s.chapter));w.U8(static_cast<std::uint8_t>(s.finalChoice));
    w.I32(s.health);w.I32(s.mana);w.I32(s.stamina);w.I32(s.concentration);w.I32(s.fatigue);
    w.U32(s.level);w.U32(s.xp);w.I64(s.wallet);w.I64(s.bank);w.I64(s.merchantTreasury);w.I64(s.rewardTreasury);
    w.U8(s.mentorMet?1:0);w.U8(s.guardianDefeated?1:0);w.U8(s.visitedFacilities);
    w.U32(s.trialHits);w.U32(s.trialDodges);w.U32(s.trialInteractions);
    for(const auto& m:s.mastery){w.U8(m.awakened?1:0);w.U32(m.xp);w.U32(m.awakenedAtLevel);w.U32(m.trainingCredits);}
    w.U32(static_cast<std::uint32_t>(s.inventory.size()));for(const auto& item:s.inventory){w.Text(item.id);w.I32(item.count);}
    w.U32(static_cast<std::uint32_t>(s.fusions.size()));for(const auto& f:s.fusions){w.U8(static_cast<std::uint8_t>(f.first));w.U8(static_cast<std::uint8_t>(f.second));w.Text(f.name);w.U32(f.xp);}
    w.U32(static_cast<std::uint32_t>(s.spells.size()));for(const auto& spell:s.spells){w.U32(spell.id);w.Text(spell.name);WriteSpec(w,spell.spec);w.U8(static_cast<std::uint8_t>(spell.phase));w.U32(spell.reproductions);}
    w.U64(s.eventSequence);w.U32(s.cooldownRemainingMs);w.U32(s.regenRemainderMs);
    w.U32(Checksum(w.bytes,w.bytes.size()));return w.bytes;
}
bool DecodeSave(const std::vector<std::uint8_t>& bytes,State& out,std::string* error) {
    if(bytes.size()<12||bytes.size()>1024*1024)return SetError(error,"Save size is invalid.");
    Reader tail{bytes,bytes.size()-4,bytes.size(),true};const auto expected=tail.U32();
    if(Checksum(bytes,bytes.size()-4)!=expected)return SetError(error,"Save checksum failed.");
    Reader r{bytes,0,bytes.size()-4,true};
    if(r.U8()!='A'||r.U8()!='F'||r.U8()!='R'||r.U8()!='S')return SetError(error,"Save magic is invalid.");
    State s;s.version=r.U32();
    if(s.version<1||s.version>CurrentSaveVersion)return SetError(error,"Unsupported save schema version.");
    s.name=r.Text(32);s.appearance.sex=r.Text(24);s.appearance.skin=r.Text(24);s.appearance.hairStyle=r.Text(24);
    s.appearance.hairColor=r.Text(24);s.appearance.eyeColor=r.Text(24);s.appearance.clothing=r.Text(32);s.appearance.bodyScale=r.Float();
    s.affinity=static_cast<Element>(r.U8());s.chapter=static_cast<Chapter>(r.U8());s.finalChoice=static_cast<FinalChoice>(r.U8());
    s.health=r.I32();s.mana=r.I32();s.stamina=r.I32();s.concentration=r.I32();s.fatigue=r.I32();
    s.level=r.U32();s.xp=r.U32();s.wallet=r.I64();s.bank=r.I64();s.merchantTreasury=r.I64();s.rewardTreasury=r.I64();
    const auto mentor=r.U8(),guardian=r.U8();if(mentor>1||guardian>1)r.good=false;s.mentorMet=mentor!=0;s.guardianDefeated=guardian!=0;s.visitedFacilities=r.U8();
    s.trialHits=r.U32();s.trialDodges=r.U32();s.trialInteractions=r.U32();
    for(auto& m:s.mastery){const auto awakened=r.U8();if(awakened>1)r.good=false;m.awakened=awakened!=0;m.xp=r.U32();m.stage=StageForXp(m.xp);
        if(s.version>=2){m.awakenedAtLevel=r.U32();m.trainingCredits=r.U32();}}
    auto count=r.U32();if(count>8)return SetError(error,"Inventory count exceeds capacity.");
    for(std::uint32_t i=0;i<count;++i){InventoryEntry item;item.id=r.Text(32);item.count=r.I32();s.inventory.push_back(item);}
    count=r.U32();if(count>MaxFusions)return SetError(error,"Fusion count exceeds capacity.");
    for(std::uint32_t i=0;i<count;++i){Fusion f;f.first=static_cast<Element>(r.U8());f.second=static_cast<Element>(r.U8());f.name=r.Text(48);f.xp=r.U32();f.stage=StageForXp(f.xp);s.fusions.push_back(f);}
    count=r.U32();if(count>MaxSpells)return SetError(error,"Spell count exceeds capacity.");
    for(std::uint32_t i=0;i<count;++i){NamedSpell spell;spell.id=r.U32();spell.name=r.Text(48);spell.spec=ReadSpec(r);
        if(s.version>=3){spell.phase=static_cast<SpellPhase>(r.U8());spell.reproductions=r.U32();}s.spells.push_back(spell);}
    if(s.version>=2)s.eventSequence=r.U64();
    if(s.version>=3){s.cooldownRemainingMs=r.U32();s.regenRemainderMs=r.U32();}
    if(!r.good||r.offset!=r.limit)return SetError(error,"Save is truncated or has trailing data.");
    if(!MigrateSave(s,error))return false;
    out=std::move(s);return true;
}

Result ApplyServerDamage(State& s,std::int32_t damage){return ApplyDamage(s,damage);}
Result ConsumeTrialCast(State& s,std::uint64_t id,StoryEvent event,std::uint64_t now) {
    if(event!=StoryEvent::TrialTargetHit&&event!=StoryEvent::TrialElementInteraction)
        return Fail(s,Error::InvalidInput,"A cast can confirm only a trial hit or elemental interaction.");
    if(s.chapter!=Chapter::FirstMagicTrial||!HasAffinity(s))return Fail(s,Error::WrongChapter,"The First Magic Trial is not active.");
    if(now<s.serverTimeMs)return Fail(s,Error::InvalidInput,"Server trial time must not move backwards.");
    auto it=std::find_if(s.pendingCasts.begin(),s.pendingCasts.end(),[&](const PendingCast& p){return p.id==id;});
    if(it==s.pendingCasts.end()||it->resolved||it->expiresAtMs<now||it->element!=s.affinity)
        return Fail(s,Error::ExpiredHit,"Use a matching-affinity, unexpired, one-use server cast.");
    State next=s;AdvanceClock(next,now);
    auto token=std::find_if(next.pendingCasts.begin(),next.pendingCasts.end(),[&](const PendingCast& p){return p.id==id;});
    token->resolved=true;
    Result result=ApplyStoryEvent(next,event);if(!result.ok)return result;
    result.castId=id;result.element=s.affinity;s=std::move(next);return result;
}
Result TreatAtHospital(State& s) {
    if(s.chapter!=Chapter::HospitalStart && !HasAffinity(s)) return Fail(s,Error::Unattuned,"Hospital treatment requires the opening chapter or an awakened patient.");
    if(s.health==100 && s.mana==100 && s.stamina==100 && s.concentration==100 && s.fatigue==0) return Fail(s,Error::InvalidInput,"You do not need treatment.");
    const bool emergency=s.health==0;
    const auto cost=(s.chapter==Chapter::HospitalStart || emergency)?0:HospitalTreatmentGoldCost;
    if(s.wallet<cost || s.merchantTreasury>MaxCoins-cost) return Fail(s,Error::InsufficientFunds,"Hospital treatment costs 20 gold.");
    s.wallet-=cost; s.merchantTreasury+=cost;
    if(emergency){s.health=50;s.mana=50;s.stamina=50;s.concentration=75;s.fatigue=20;}
    else{s.health=100;s.mana=100;s.stamina=100;s.concentration=100;s.fatigue=0;}
    return Ok(s,emergency?"Hospital staff stabilize your knockout. Magic itself cannot resurrect.":"Hospital staff complete your treatment.");
}
Result RestAtInn(State& s) {
    if(!EconomyOpen(s)) return Fail(s,Error::WrongChapter,"The inn opens during the city introduction.");
    if(s.health<=0) return Fail(s,Error::Unstable,"An inn cannot treat a knockout. Seek hospital staff.");
    if(s.health==100 && s.mana==100 && s.stamina==100 && s.concentration==100 && s.fatigue==0) return Fail(s,Error::InvalidInput,"You are already rested.");
    if(s.wallet<InnRestGoldCost || s.merchantTreasury>MaxCoins-InnRestGoldCost) return Fail(s,Error::InsufficientFunds,"An inn rest costs 15 gold.");
    s.wallet-=InnRestGoldCost;s.merchantTreasury+=InnRestGoldCost;s.health=std::min(100,s.health+30);s.mana=100;s.stamina=100;s.concentration=100;s.fatigue=0;
    return Ok(s,"The paid inn rest restores your bounded resources.");
}
} } // namespace Aetheria::Rules
