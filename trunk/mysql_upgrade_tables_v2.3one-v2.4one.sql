ALTER TABLE bans ADD gamecount INT DEFAULT '0' NOT NULL;
ALTER TABLE bans ADD expiredate TEXT NOT NULL;
ALTER TABLE bans ADD warn INTEGER DEFAULT '0' NOT NULL;