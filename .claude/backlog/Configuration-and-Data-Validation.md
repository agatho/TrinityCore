# Playerbot Configuration and Data Validation Backlog

## Critical Integration Tasks

### 1. Configuration File Migration - HIGH PRIORITY

**Objective:** Ensure proper transposition of playerbots.conf.dist from mod-playerbot source

**Tasks:**
- [ ] **Locate Original Config**: Find playerbots.conf.dist in /workspace/mod-playerbots
- [ ] **Analyze Configuration Options**: Document all configuration parameters and their purposes
- [ ] **Map to TrinityCore Standards**: Adapt configuration format to TrinityCore conventions
- [ ] **Create New Config File**: Generate playerbots.conf.dist for TrinityCore integration
- [ ] **Validate All Options**: Ensure every configuration option has proper default values
- [ ] **Add New Scale Options**: Include configuration for 5000+ bot deployment settings

**Configuration Categories to Validate:**
```
[PLAYERBOT]
# Bot Management
PlayerbotMgr.enabled = 1
PlayerbotMgr.maxBotsPerPlayer = 9
PlayerbotMgr.restrictBotLevel = 80
PlayerbotMgr.randomBotLoginInterval = 60

# AI Behavior
PlayerbotAI.allowPlayerBots = 1
PlayerbotAI.botActiveAlone = 0
PlayerbotAI.randomBotJoinLfg = 1
PlayerbotAI.randomBotJoinBG = 1

# Database Integration
PlayerbotDB.enabled = 1
PlayerbotDB.connectionPoolSize = 10
PlayerbotDB.batchUpdateInterval = 5000

# Performance Settings (NEW - for 5000+ bots)
PlayerbotPerformance.maxConcurrentBots = 5000
PlayerbotPerformance.threadPoolSize = 32
PlayerbotPerformance.memoryLimitMB = 10240
```

**Priority Level:** CRITICAL - Required for proper bot initialization and configuration

---

### 2. DBC/DB2 Data Validation - HIGH PRIORITY

**Objective:** Ensure all DBC/DB2 data requirements use current WoW client data

**Background:**
- Original mod-playerbots may reference outdated DBC structures
- Current TrinityCore uses updated client data formats
- Mismatched data can cause crashes or incorrect behavior

**Validation Areas:**

#### A. Spell Data Validation
**Files to Check:**
- All references to `SpellInfo`, `SpellEntry`, `SpellMgr` classes
- Recipe and crafting spell lookups in PlayerbotProfessionManager
- Combat spell usage in AI decision systems

**Validation Tasks:**
- [ ] **Spell ID Mapping**: Verify all hardcoded spell IDs are current
- [ ] **Recipe System**: Validate crafting recipe spell references
- [ ] **Combat Abilities**: Check class ability spell IDs and mechanics
- [ ] **Talent System**: Ensure talent tree references match current client

#### B. Item Data Validation  
**Files to Check:**
- PlayerbotItemOptimizer item template usage
- PlayerbotAuctionManager market data references
- All hardcoded item IDs throughout the codebase

**Validation Tasks:**
- [ ] **Item Template Structure**: Verify ItemTemplate field usage matches current
- [ ] **Item Stats**: Validate stat calculations use current item stat systems
- [ ] **Equipment Slots**: Confirm equipment slot mappings are current
- [ ] **Item Categories**: Check item class/subclass enumerations

#### C. Quest Data Validation
**Files to Check:**
- PlayerbotQuestManager quest template usage
- Quest objective and reward parsing
- Quest chain dependency tracking

**Validation Tasks:**
- [ ] **Quest Template**: Verify QuestTemplate structure usage
- [ ] **Objective Types**: Validate quest objective type enumerations
- [ ] **Reward Systems**: Check quest reward parsing and evaluation
- [ ] **Quest Chains**: Ensure quest prerequisite system is current

#### D. Achievement Data Validation
**Files to Check:**
- PlayerbotAchievementManager achievement entry usage
- Achievement criteria parsing and tracking
- Meta-achievement dependency handling

**Validation Tasks:**
- [ ] **Achievement Structure**: Verify AchievementEntry field usage
- [ ] **Criteria Types**: Validate achievement criteria type enumerations  
- [ ] **Progress Tracking**: Ensure progress tracking matches current system
- [ ] **Reward Processing**: Check achievement reward handling

#### E. Zone and Map Data Validation
**Files to Check:**
- All references to zone IDs, area IDs, and map data
- Pathfinding and movement system integration
- Exploration and location-based AI decisions

**Validation Tasks:**
- [ ] **Zone Mapping**: Verify zone and area ID references are current
- [ ] **Map Coordinates**: Validate coordinate system usage
- [ ] **Pathfinding Integration**: Ensure movement maps are compatible
- [ ] **Instance Data**: Check dungeon and instance ID mappings

#### F. Faction and Reputation Data
**Files to Check:**
- Social system faction references
- Reputation-based AI decisions
- Guild and group faction handling

**Validation Tasks:**
- [ ] **Faction IDs**: Verify faction ID references are current
- [ ] **Reputation System**: Validate reputation calculation methods
- [ ] **Standing Levels**: Check reputation standing enumerations
- [ ] **Faction Relationships**: Ensure faction relationship data is current

---

### 3. Database Schema Validation - MEDIUM PRIORITY

**Objective:** Ensure database table structures match current TrinityCore standards

**Validation Tasks:**
- [ ] **Character Tables**: Verify character database integration points
- [ ] **World Tables**: Check world database references for NPCs, items, quests
- [ ] **Custom Tables**: Validate any custom playerbot tables are properly structured
- [ ] **Foreign Keys**: Ensure all foreign key relationships are valid

---

### 4. API Compatibility Validation - MEDIUM PRIORITY  

**Objective:** Verify all TrinityCore API usage is current

**Areas to Validate:**
- [ ] **Player Class Methods**: Ensure Player class method calls are current
- [ ] **World Session Handling**: Verify WorldSession integration points
- [ ] **Database API**: Check CharacterDatabase and WorldDatabase usage
- [ ] **Spell System Integration**: Validate Spell and SpellInfo API usage
- [ ] **Item System Integration**: Check Item and ItemTemplate API usage

---

## Implementation Priority

### Phase 1: Critical Configuration (Week 1)
1. Locate and analyze original playerbots.conf.dist
2. Create TrinityCore-compatible configuration file
3. Add 5000+ bot scale configuration options
4. Validate all configuration parameter mappings

### Phase 2: Data Structure Validation (Week 2-3)
1. Systematic validation of all DBC/DB2 references
2. Spell, item, quest, and achievement data verification
3. Update any outdated data structure usage
4. Test data compatibility with current client

### Phase 3: API Compatibility (Week 4)
1. Comprehensive API usage validation
2. Update deprecated method calls
3. Ensure database integration compatibility
4. Final integration testing

---

## Risk Assessment

### High Risk Areas
- **Spell System Changes**: Spell mechanics may have changed significantly
- **Item Stat System**: Item stat calculations may use different formulas
- **Quest System Updates**: Quest objective types and mechanics may be updated
- **Database Schema Changes**: Core table structures may have evolved

### Mitigation Strategies
- **Incremental Validation**: Validate one system at a time
- **Comprehensive Testing**: Test each validated system thoroughly
- **Fallback Options**: Maintain ability to disable problematic features
- **Documentation**: Document all discovered changes and required updates

---

## Success Criteria

### Configuration File
- ✅ All original mod-playerbots configuration options properly mapped
- ✅ New 5000+ bot scale options added and validated  
- ✅ TrinityCore configuration format standards followed
- ✅ All default values tested and working

### Data Validation
- ✅ All DBC/DB2 references use current WoW client data
- ✅ No crashes or errors from outdated data structure usage
- ✅ All hardcoded IDs verified as current and valid
- ✅ Performance impact of data access patterns optimized

### API Compatibility  
- ✅ All TrinityCore API usage is current and compatible
- ✅ No deprecated method calls or obsolete integrations
- ✅ Database integration follows current patterns
- ✅ Memory and resource usage patterns are optimized

---

**Timeline:** 4 weeks parallel to Phase 5 implementation  
**Priority:** HIGH - Critical for stable bot deployment  
**Dependencies:** Access to both mod-playerbots and current TrinityCore codebases  
**Success Metrics:** Zero configuration-related or data-compatibility crashes