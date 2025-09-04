# Phase 2: TrinityCore-PlayerbotAI Integration Architecture Design

## 🎯 INTEGRATION STRATEGY

### Overview
Integrate mod-playerbots' sophisticated Strategy-Action AI system with TrinityCore's existing PlayerAI framework, maintaining backwards compatibility and conditional compilation.

## 🏗️ ARCHITECTURE ANALYSIS

### TrinityCore PlayerAI Framework
**Location**: `src/server/game/AI/PlayerAI/`

**Key Characteristics**:
- **Base Class**: `PlayerAI` extends `UnitAI`
- **Design Pattern**: Template method with virtual overrides
- **Focus**: Charmed/controlled player behavior
- **Main Classes**:
  - `PlayerAI` - Base class with spell verification and target selection
  - `SimpleCharmedPlayerAI` - Concrete implementation with `UpdateAI(uint32 diff)`

**Core Methods**:
- `UpdateAI(uint32 diff)` - Main update cycle (virtual, overrideable)
- `SelectAttackTarget()` - Target selection logic  
- `VerifySpellCast()` - Spell validation and casting
- `DoRangedAttackIfReady()` - Ranged attack logic

### mod-playerbots Engine-Strategy-Action System
**Location**: `/workspace/mod-playerbots/src/strategy/`

**Key Characteristics**:
- **Design Pattern**: Strategy-Action with Queue-based execution
- **Core Classes**:
  - `Engine` - Central AI processor with strategy execution
  - `Strategy` - Behavioral strategy with triggers and actions  
  - `Action` - Individual executable actions with prerequisites
  - `Trigger` - Event-based condition system
  - `Value` - Dynamic data with caching
  - `AiObjectContext` - Dependency injection container

**Core Engine Flow**:
1. `ProcessTriggers(minimal)` - Evaluate all triggers
2. `PushDefaultActions()` - Add default behaviors to queue
3. **Action Queue Processing**:
   - Pick highest relevance action from priority queue
   - Execute action if prerequisites met
   - Handle execution listeners and result processing

## 🔗 INTEGRATION APPROACH

### 1. Hybrid PlayerAI Architecture

**Create Enhanced PlayerAI Class**:
```cpp
#ifdef WITH_PLAYERBOTS
class TC_GAME_API PlayerbotPlayerAI : public PlayerAI
{
public:
    PlayerbotPlayerAI(Player* player);
    ~PlayerbotPlayerAI();
    
    // Override main update method
    void UpdateAI(uint32 diff) override;
    
    // Engine integration
    void InitializeEngine();
    void SetStrategy(std::string const& strategy);
    
private:
    std::unique_ptr<Engine> m_engine;
    std::unique_ptr<AiObjectContext> m_context;
    bool m_initialized;
};
#endif
```

### 2. Engine Integration Pattern

**Engine Initialization**:
- Create `Engine` instance in `PlayerbotPlayerAI` constructor
- Initialize `AiObjectContext` with TrinityCore-specific factories
- Load default strategies based on player class/spec

**UpdateAI Override**:
```cpp
void PlayerbotPlayerAI::UpdateAI(uint32 diff)
{
    if (!m_initialized)
        return;
        
    // Call mod-playerbots Engine logic
    m_engine->DoNextAction(me, 0, false);
    
    // Fallback to base PlayerAI for unhandled cases
    if (!actionWasExecuted)
        PlayerAI::UpdateAI(diff);
}
```

### 3. Strategy-Action Porting Strategy

**Phase 2.2: Port Core Framework**
1. **Engine Class**: Port `Engine.h/.cpp` with TrinityCore adaptations
2. **Strategy Base**: Port `Strategy.h/.cpp` framework  
3. **Action Base**: Port `Action.h/.cpp` system
4. **AiObjectContext**: Port dependency injection system

**Conditional Compilation Pattern**:
```cpp
#ifdef WITH_PLAYERBOTS
// mod-playerbots integration code
#include "Engine.h"
#include "Strategy.h"
#include "Action.h"
#else 
// Standard TrinityCore PlayerAI behavior
#endif
```

### 4. TrinityCore API Adaptation Layer

**ServerFacade Integration**:
- Port `ServerFacade.h/.cpp` to bridge mod-playerbots API calls to TrinityCore
- Map AzerothCore-specific calls to TrinityCore equivalents
- Handle API differences between server implementations

**Example Adaptations**:
```cpp
// mod-playerbots calls -> TrinityCore equivalents
botAI->GetBot()->GetGUID() -> me->GetGUID() 
botAI->GetBot()->GetMap() -> me->GetMap()
// Handle differences in spell casting, movement, etc.
```

## 📂 FILE STRUCTURE PLAN

### Target Integration Points
```
src/server/game/AI/PlayerAI/
├── PlayerAI.h                    # Existing - add PlayerbotPlayerAI declaration
├── PlayerAI.cpp                  # Existing - add PlayerbotPlayerAI implementation  
└── Playerbots/                   # New directory for playerbot-specific AI
    ├── PlayerbotEngine.h/.cpp    # Port of Engine with TrinityCore integration
    ├── PlayerbotStrategy.h/.cpp  # Port of Strategy framework
    ├── PlayerbotAction.h/.cpp    # Port of Action system
    ├── PlayerbotContext.h/.cpp   # Port of AiObjectContext
    ├── PlayerbotTrigger.h/.cpp   # Port of Trigger system
    ├── PlayerbotValue.h/.cpp     # Port of Value system
    └── ServerFacade.h/.cpp       # TrinityCore API adaptation layer
```

### Integration with Existing Infrastructure
- **Script Hooks**: PlayerbotScript hooks call `PlayerbotPlayerAI::UpdateAI()`
- **Database**: Use existing PlayerbotsDatabaseConnection for configuration
- **Conditional Compilation**: All playerbot AI code behind `#ifdef WITH_PLAYERBOTS`

## 🔄 EXECUTION PLAN

### Phase 2.2: Port Engine Class (Week 1-2)
1. **Study Integration Points**: Map Engine dependencies to TrinityCore
2. **Port Engine.h/.cpp**: Create `PlayerbotEngine` adapted for TrinityCore
3. **Create PlayerbotPlayerAI**: Extend PlayerAI with Engine integration
4. **Basic Testing**: Ensure compilation and basic instantiation

### Phase 2.3: Port Strategy-Action Framework (Week 3-4)  
1. **Port Strategy System**: Adapt Strategy.h/.cpp for TrinityCore
2. **Port Action System**: Adapt Action.h/.cpp with TrinityCore API calls
3. **Port Context System**: Create AiObjectContext for dependency injection
4. **Basic Actions**: Implement simple actions (movement, basic combat)

### Phase 2.4: Session Management Integration (Week 5-6)
1. **Player Integration**: Ensure proper Player* handling in bot AI
2. **Update Loop Integration**: Connect with PlayerbotScript::OnPlayerbotUpdate()
3. **Memory Management**: Proper lifecycle management of AI components
4. **Configuration**: Load strategies from database/config

## ✅ SUCCESS CRITERIA

### Phase 2.2 Complete:
- [ ] Engine class successfully ported and compiling
- [ ] PlayerbotPlayerAI extends PlayerAI correctly
- [ ] Basic Engine::DoNextAction() executes without errors
- [ ] Conditional compilation working (WITH_PLAYERBOTS flag)

### Phase 2.3 Complete:
- [ ] Strategy-Action framework operational
- [ ] Basic bot actions execute (movement, target selection)
- [ ] AiObjectContext provides dependency injection
- [ ] Bot can execute simple behavioral strategies

### Phase 2.4 Complete:
- [ ] Bot Player objects managed correctly
- [ ] Integration with PlayerbotScript hooks working  
- [ ] Multiple bots can run simultaneously
- [ ] Database configuration loading

## 🚨 RISK MITIGATION

1. **API Compatibility**: Create comprehensive ServerFacade adaptation layer
2. **Memory Management**: Use RAII and smart pointers for bot AI components  
3. **Performance**: Profile integration to ensure minimal impact on server performance
4. **Conditional Compilation**: Ensure zero overhead when `WITH_PLAYERBOTS=OFF`
5. **Testing**: Incremental testing at each integration milestone

## 🔍 TECHNICAL CHALLENGES

1. **API Mapping**: AzerothCore vs TrinityCore API differences
2. **Threading**: Ensure thread-safety in multi-bot scenarios  
3. **Memory**: Efficient management of AI objects for multiple bots
4. **Integration**: Seamless integration with existing PlayerAI without conflicts

This design provides a clear roadmap for integrating the sophisticated mod-playerbots AI system with TrinityCore's existing PlayerAI framework while maintaining code quality and backwards compatibility.