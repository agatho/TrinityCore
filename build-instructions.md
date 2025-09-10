TRINITYCORE PLAYERBOT BUILD AND QUALITY ASSURANCE

CRITICAL RULES:
- NEVER clean the build directory
- Use incremental build log naming: build.log.$ (check existing logs and start with next number)
- NO workarounds - fix root causes only
- TrinityCore original code is the golden source - follow its patterns and methods
- NO timeouts when executing commands
- Speed is not priority - correctness is
- Coding Guidelines: https://trinitycore.info/en/contributing/c++-development-standards
- Fallback Coding Guidelines: https://google.github.io/styleguide/cppguide.html
- Always document you work.

PHASE 1: ERROR RESOLUTION
1. Analyze all errors in the current build log
2. Create a comprehensive remediation plan categorizing errors by type and priority
3. Fix errors systematically following the plan:
   - Use TrinityCore's existing methods and functions
   - Maintain original architecture patterns
   - Fix root causes, not symptoms
   - For Fixes regarding databases make sure that the porper sql files exist or create them (playerbot.sql , playerbot_characters.sql, playerbot_game.sql) and verify that that all statements align with modern trinitycore patterns.
4. Document ans save each fix with explanation of the problem and solution
5. Run incremental test builds (do NOT clean build directory)
6. Repeat until core compiles without errors

PHASE 2: API MIGRATION AND DATABASE ALIGNMENT
After successful compilation:

2.1 TRINITYCORE API MIGRATION
1. Systematically review ALL files in playerbots/ and subdirectories
2. Identify and replace old mod-playerbot API calls with current TrinityCore API:
   - Check function signatures, parameter types, and return values
   - Update deprecated methods to current TrinityCore equivalents
   - Ensure proper include headers for TrinityCore classes
   - Verify object lifecycle management follows TrinityCore patterns
3. Document and save each API migration with before/after examples
4. Test critical functionality paths after API updates

2.2 DATABASE STRUCTURE ALIGNMENT
1. Review all SQL statements throughout the playerbot codebase
2. Update database queries to match current TrinityCore database structure:
   - Table names, column names, and data types
   - Foreign key relationships and constraints
   - Index usage and query optimization
3. Migrate all database operations to use TrinityCore's PlayerBot DatabaseManager:
   - Replace direct SQL calls with DatabaseManager methods
   - Ensure proper transaction handling
   - Follow TrinityCore's database access patterns
4. Verify all database operations are compatible with current schema

2.3 QUALITY ASSURANCE
1. Conduct comprehensive code quality review of modified components
2. Align code with TrinityCore coding guidelines and best practices
3. Ensure proper error handling and logging throughout
4. Document all migrations and improvements made

Critical: Without proper API and database migration, playerbot functionality will be non-functional. Priority is correctness over speed.

PHASE 3: QUALITY ASSURANCE
After successful compilation:
1. Conduct comprehensive code quality review of ALL playerbot components
2. Align code with TrinityCore coding guidelines and best practices
3. Fix any style, structure, or implementation issues found
4. Document all quality improvements made

PHASE 4: VERSION CONTROL
1. Get the last commit message from playerbot branch
2. Analyze all changes made during phases 1-2
3. Create logical commit packages based on project phases:
   - Group related fixes together
   - Separate error fixes from quality improvements
   - Each package should be atomic and buildable
4. Commit packages sequentially with detailed, professional commit messages
5. Each commit message should explain WHAT was changed and WHY

Execute phases sequentially. Do not proceed to next phase until current phase is complete.