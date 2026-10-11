#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace Aetheria { namespace Rules {
constexpr std::size_t ElementCount = 11;
constexpr std::size_t StageCount = 6;
constexpr std::uint32_t CurrentSaveVersion = 3;
constexpr std::size_t MaxSpells = 32;
constexpr std::size_t MaxFusions = 16;
constexpr std::int64_t TrainingGoldCost = 20;
constexpr std::int64_t HospitalTreatmentGoldCost = 20;
constexpr std::int64_t InnRestGoldCost = 15;
constexpr std::int64_t ManaPotionGoldCost = 25;
constexpr std::int64_t HealthPotionGoldCost = 30;
constexpr std::int32_t TrainingManaCost = 8;

enum class Element : std::uint8_t { Fire, Water, Air, Earth, Space, Shadow, Dark, Light, Electric, Summon, Healing, None = 255 };
enum class Stage : std::uint8_t { Basic, Real, High, Advanced, Super, Perfect };
enum class Chapter : std::uint8_t { HospitalStart, AwakeningHall, FirstMagicTrial, GuardianEncounter, CityVisits, FinalDecision, Complete };
enum class Facility : std::uint8_t { Hospital, Academy, GuildHall, Houses, Market, Bank, Inn };
enum class FinalChoice : std::uint8_t { None, Seal, Study };
enum class StoryEvent : std::uint8_t { MeetDoctor, MeetSeraphine, TrialTargetHit, TrialDodge, TrialElementInteraction, GuardianDefeated, VisitHospital, VisitAcademy, VisitGuildHall, VisitHouses, VisitMarket, VisitBank, VisitInn, ReturnToSeraphine };
enum class Error : std::uint8_t { None, InvalidInput, WrongChapter, AlreadyCompleted, Unattuned, WrongAffinity, InsufficientMastery, InsufficientMana, InsufficientFunds, Cooldown, Unstable, Capacity, Duplicate, InvalidSave, ExpiredHit, MissingTraining };
enum class Form : std::uint8_t { Projectile, Stream, Wall, Shield, Displacement, Construct, Restore };
enum class Shape : std::uint8_t { Sphere, Cone, Plane, Line, Humanoid };
enum class Delivery : std::uint8_t { Bolt, Channel, Placed, Self };
enum class Behavior : std::uint8_t { Impact, Sustain, Protect, Move, Follow, Heal };
enum class Target : std::uint8_t { Enemy, Self, Ally, Ground };
enum class SecondaryEffect : std::uint8_t { None, Slow, Burn, Stagger, Weaken };
enum class SpellPhase : std::uint8_t { Experiment, Stabilized, Reproduced, Registered };
enum class GuardianPhase : std::uint8_t { One, Two, Defeated };

struct Appearance {
    std::string sex = "female", skin = "warm", hairStyle = "short", hairColor = "brown";
    std::string eyeColor = "brown", clothing = "traveler";
    float bodyScale = 1.0f;
};
struct Mastery {
    bool awakened = false;
    std::uint32_t xp = 0;
    Stage stage = Stage::Basic;
    std::uint32_t awakenedAtLevel = 0;
    std::uint32_t trainingCredits = 0;
};
struct SpellSpec {
    Element element = Element::Fire;
    Form form = Form::Projectile;
    Shape shape = Shape::Sphere;
    Delivery delivery = Delivery::Bolt;
    Behavior behavior = Behavior::Impact;
    float range = 12.0f, speed = 18.0f, intensity = 1.0f, duration = 0.0f;
    Target target = Target::Enemy;
    SecondaryEffect secondary = SecondaryEffect::None;
    std::int32_t fusionIndex = -1;
};
struct NamedSpell {
    std::uint32_t id = 0;
    std::string name;
    SpellSpec spec;
    SpellPhase phase = SpellPhase::Experiment;
    std::uint32_t reproductions = 0;
};
struct Fusion {
    Element first = Element::Fire, second = Element::Earth;
    std::string name;
    Stage stage = Stage::Basic;
    std::uint32_t xp = 0;
};
struct InventoryEntry { std::string id; std::int32_t count = 0; };
struct PendingCast {
    std::uint64_t id = 0, expiresAtMs = 0;
    std::int32_t damage = 0;
    Element element = Element::None;
    bool resolved = false;
};
struct State {
    std::uint32_t version = CurrentSaveVersion;
    std::string name = "Aerin";
    Appearance appearance;
    Element affinity = Element::None;
    Chapter chapter = Chapter::HospitalStart;
    FinalChoice finalChoice = FinalChoice::None;
    std::int32_t health = 100, mana = 100, stamina = 100, concentration = 100, fatigue = 0;
    std::array<Mastery, ElementCount> mastery{};
    std::uint32_t level = 1, xp = 0;
    std::int64_t wallet = 250, bank = 0, merchantTreasury = 10000, rewardTreasury = 50000;
    std::vector<InventoryEntry> inventory;
    std::vector<NamedSpell> spells;
    std::vector<Fusion> fusions;
    std::uint32_t trialHits = 0, trialDodges = 0, trialInteractions = 0;
    std::uint8_t visitedFacilities = 0;
    bool mentorMet = false, guardianDefeated = false;
    std::uint64_t lastCastAtMs = 0, serverTimeMs = 0, eventSequence = 0;
    std::uint32_t cooldownRemainingMs = 0;
    std::uint32_t regenRemainderMs = 0;
    std::vector<PendingCast> pendingCasts;
};
struct GuardianState {
    std::int32_t health = 240, maxHealth = 240;
    GuardianPhase phase = GuardianPhase::One;
    bool phaseTwoReached = false;
};
struct Result {
    bool ok = false;
    Error error = Error::None;
    std::string message;
    std::int32_t manaCost = 0, damage = 0, healing = 0;
    Element element = Element::None;
    Chapter chapter = Chapter::HospitalStart;
    std::uint64_t castId = 0;
};

const char* ElementName(Element element);
const char* StageName(Stage stage);
Stage StageForXp(std::uint32_t xp);
State CreateNewPlayer(const std::string& name);
SpellSpec DefaultSpell(Element element);
std::string GetObjective(const State& state);
Result ApplyStoryEvent(State& state, StoryEvent event);
Result ConsumeTrialCast(State& state, std::uint64_t castId, StoryEvent event, std::uint64_t serverNowMs);
Result Awaken(State& state, std::uint32_t serverRandom);
Result ChooseFinal(State& state, FinalChoice choice);
Result Train(State& state, Element element);
std::int32_t GetSpellCost(const State& state, const SpellSpec& spec);
Result ValidateSpell(const State& state, const SpellSpec& spec);
Result Cast(State& state, const SpellSpec& spec, std::uint64_t serverNowMs);
Result ExperimentSpell(State& state, const SpellSpec& spec, std::uint64_t serverNowMs);
Result StabilizeSpell(State& state, std::uint32_t spellId);
Result ReproduceSpell(State& state, std::uint32_t spellId, std::uint64_t serverNowMs);
Result RegisterSpell(State& state, const SpellSpec& spec, const std::string& name);
Result Fuse(State& state, Element first, Element second);
Result Deposit(State& state, std::int64_t amount);
Result Withdraw(State& state, std::int64_t amount);
Result BuyItem(State& state, const std::string& itemId, std::int32_t quantity);
Result UseItem(State& state, const std::string& itemId);
Result TreatAtHospital(State& state);
Result RestAtInn(State& state);
Result Tick(State& state, std::uint32_t elapsedMs);
Result ApplyDamage(State& state, std::int32_t trustedServerDamage);
Result ApplyServerDamage(State& state, std::int32_t trustedServerDamage);
Result SpendStamina(State& state, std::int32_t amount);
Result ConfirmGuardianDefeat(State& state, const GuardianState& guardian);
GuardianState CreateGuardian();
Result ResolveGuardianHit(State& state, GuardianState& guardian, std::uint64_t castId, std::uint64_t serverNowMs);
bool ValidateState(const State& state, std::string* error = nullptr);
bool MigrateSave(State& state, std::string* error = nullptr);
std::vector<std::uint8_t> EncodeSave(const State& state);
bool DecodeSave(const std::vector<std::uint8_t>& bytes, State& out, std::string* error = nullptr);
} } // namespace Aetheria::Rules
