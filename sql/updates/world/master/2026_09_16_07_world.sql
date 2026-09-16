-- Register sql/content for the updater (same idempotent row as the content branches), so sql/content/Lorewalking applies
INSERT IGNORE INTO `updates_include` (`path`, `state`) VALUES ('$/sql/content', 'RELEASED');
