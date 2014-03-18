---------------------------
	-- user access
user
	-		/home				-- dont see other users
	+rw		/home/%user			-- see itself
	+r		/sys/fs/smackfs		-- disable write access to change-rule
	
---------------------------
-- basic restricted access
restricted
	-		/home								-- dont see any other user
	+rw		/home/%user/.config/%appid			-- access to config
	+r		/home/%user/share					-- shared data
	+rw		/home/%user/share/%appid			-- own shared data
	+rw		/home/%user/share/.cert/%cert		-- same certificate
	+r		/sys/fs/smackfs						-- disable change-rule


