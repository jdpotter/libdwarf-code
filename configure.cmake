include(AutoconfHelper)

ac_c_bigendian()
ac_check_headers(sys/types.h sys/stat.h stdlib.h string.h memory.h strings.h inttypes.h stdint.h unistd.h)

# libdwarf default-disabled shared
option(shared "build shared library libdwarf.so and use it if present" FALSE)

option(nonshared "build archive library libdwarf.a" TRUE)

#  This adds compiler option -Wall (gcc compiler warnings)
#  'set' here uses $< introducing what is called 
#  a 'generator expression' in cmake documentation.
option(wall "Add -Wall" FALSE)
set(dwfwall $<$<BOOL:${wall}>:"-Wall -O0 -Wpointer-arith -Wmissing-declarations -Wmissing-prototypes -Wdeclaration-after-statement -Wextra -Wcomment -Wformat -Wpedantic -Wuninitialized -Wno-long-long -Wshadow">)

option(nonstandardprintf "Use a special printf format for 64bit (default is NO)" FALSE)
set(HAVE_NONSTANDARD_PRINTF_64_FORMAT ${nonstandardprintf} )
message(STATUS "Checking enable nonstandardprintf... ${HAVE_NONSTANDARD_PRINTF_64_FORMAT}")

option(libelf "Use libelf (default is YES)" TRUE)
set(DWARF_WITH_LIBELF ${libelf} )
message(STATUS "Building using libelf... ${DWARF_WITH_LIBELF}")

option(dodwarfgen "Build dwarfgen (default is NO)" FALSE)
set(BUILD_DWARFGEN ${dodwarfgen} )
message(STATUS "Building dwarfgen    ... ${BUILD_DWARFGEN}")

option(dodwarfexample "Build dwarfexample (default is NO)" FALSE)
set(BUILD_DWARFEXAMPLE ${dodwarfexample} )
message(STATUS "Building dwarfexample... ${BUILD_DWARFEXAMPLE}")

option(test "Do certain api tests (default is NO)" FALSE)
set(DO_TESTING ${test} )
message(STATUS "Building api tests   ... ${DOTESTS}")
