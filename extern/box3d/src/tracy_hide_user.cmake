# Added for Nebenan: runs in the source of Tracy 0.13.1 after it is fetched, see BOX3D_PROFILE. The client writes the
# login and host name of the machine into the host information of every capture. It writes "(hidden)" instead, so a
# capture can be shared. Running it again changes nothing.
set(file "public/client/TracyProfiler.cpp")
file(READ "${file}" text)
string(REPLACE [=[ptr += sprintf( ptr, "User: %s@%s\n", user, hostname );]=]
	[=[ptr += sprintf( ptr, "User: (hidden)\n" ); (void)user; (void)hostname;]=] text "${text}")
string(REPLACE [=[ptr += sprintf( ptr, "User: %s@%s\n", login, hostname );]=]
	[=[ptr += sprintf( ptr, "User: (hidden)\n" ); (void)login; (void)hostname;]=] text "${text}")
string(REPLACE [=[ptr += sprintf( ptr, "User: (?)@%s\n", hostname );]=]
	[=[ptr += sprintf( ptr, "User: (hidden)\n" ); (void)hostname;]=] text "${text}")
string(FIND "${text}" [=["User: %s@%s\n"]=] left)
if(NOT left EQUAL -1)
	message(FATAL_ERROR "Tracy still writes the user and host name, see tracy_hide_user.cmake")
endif()
file(WRITE "${file}" "${text}")
