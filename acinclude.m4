
# QL_CHECK_GCC_VERSION
# --------------------
# Abort configure if the compiler is a GCC older than 15.2.1, the fork's
# floor, with the message CMakeLists.txt gives.  Like CMake, it checks
# GCC only: Clang defines __GNUC__ too (as 4.2.1), so it is excluded by
# name, as is classic Intel C++.  The preprocessor decides; the
# -dumpfullversion call only fills in the error message.
AC_DEFUN([QL_CHECK_GCC_VERSION],
[AC_MSG_CHECKING([for GCC version >= 15.2.1])
 AC_COMPILE_IFELSE(
    [AC_LANG_PROGRAM(
        [[@%:@if !defined(__GNUC__) || defined(__clang__) || defined(__INTEL_COMPILER)
          @%:@error not GCC
          @%:@endif
          ]],
        [[]])],
    [AC_COMPILE_IFELSE(
        [AC_LANG_PROGRAM(
            [[@%:@if __GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__ < 150201
              @%:@error GCC older than 15.2.1
              @%:@endif
              ]],
            [[]])],
        [AC_MSG_RESULT([yes])],
        [AC_MSG_RESULT([no])
         ql_gcc_version=`$CXX -dumpfullversion 2>/dev/null`
         AC_MSG_ERROR([QuantLib requires GCC 15.2.1 or higher; found ${ql_gcc_version:-an older version}])
        ])],
    [AC_MSG_RESULT([not GCC])])
])

# QL_CHECK_CPP23
# --------------------
# Check whether C++23 features are supported by default.  If not, retry
# with -std=c++23 added; if that still fails, abort configure.  This fork
# requires C++23, and QL_CHECK_GCC_VERSION above enforces its GCC floor;
# silently tolerating an older standard via flag prepend (which last
# -std= wins over) hid a CI/floor divergence in the past.
AC_DEFUN([QL_CHECK_CPP23],
[AC_MSG_CHECKING([for C++23 support])
 AC_COMPILE_IFELSE(
    [AC_LANG_PROGRAM(
        [[@%:@include <expected>
          void foo() {
              auto x = std::expected<int, int>{42};
              (void)x; // avoids unused-variable warning
          }
          ]],
        [[]])],
    [AC_MSG_RESULT([yes])],
    [ql_saved_CXXFLAGS=$CXXFLAGS
     CXXFLAGS="-std=c++23 ${CXXFLAGS}"
     AC_COMPILE_IFELSE(
        [AC_LANG_PROGRAM(
            [[@%:@include <expected>
              void foo() {
                  auto x = std::expected<int, int>{42};
                  (void)x;
              }
              ]],
            [[]])],
        [AC_MSG_RESULT([yes (with -std=c++23)])
         AC_SUBST([CPP23_CXXFLAGS],["-std=c++23"])
         AC_SUBST([CXXFLAGS],["-std=c++23 ${ql_saved_CXXFLAGS}"])],
        [CXXFLAGS=$ql_saved_CXXFLAGS
         AC_MSG_RESULT([no])
         AC_MSG_ERROR([this fork requires a C++23 toolchain (GCC >= 15.2.1 or equivalent Clang).  <expected> is not available with the current compiler.])
        ])
    ])
])

# QL_CHECK_SYSTEM_HEADER_PREFIX
# -----------------------------
# Check whether the compiler supports the --system-header-prefix flag
AC_DEFUN([QL_CHECK_SYSTEM_HEADER_PREFIX],
[AC_MSG_CHECKING([if ${CXX} supports --system-header-prefix])
 AC_REQUIRE([AC_PROG_CC])
 ql_original_CXXFLAGS=$CXXFLAGS
 CXXFLAGS="$ql_original_CXXFLAGS --system-header-prefix=boost/"
 AC_COMPILE_IFELSE(
    [AC_LANG_PROGRAM(
        [[int foo() { return 0; }]],
        [[]])],
    [AC_MSG_RESULT([yes])
     AC_SUBST([SYSTEM_HEADER_CXXFLAGS],["--system-header-prefix=boost/"])
    ],
    [AC_MSG_RESULT([no])])
 CXXFLAGS="$ql_original_CXXFLAGS ${SYSTEM_HEADER_CXXFLAGS}"
])

# QL_CHECK_BOOST_DEVEL
# --------------------
# Check whether the Boost headers are available
AC_DEFUN([QL_CHECK_BOOST_DEVEL],
[AC_MSG_CHECKING([for Boost development files])
 AC_COMPILE_IFELSE(
    [AC_LANG_PROGRAM(
        [[@%:@include <boost/version.hpp>
          @%:@include <boost/shared_ptr.hpp>
          @%:@include <boost/assert.hpp>
          @%:@include <boost/current_function.hpp>]],
        [[]])],
    [AC_MSG_RESULT([yes])],
    [AC_MSG_RESULT([no])
     AC_MSG_ERROR([Boost development files not found])
    ])
])

# QL_CHECK_BOOST_VERSION
# ----------------------
# Check whether the Boost installation is 1.83 or higher
AC_DEFUN([QL_CHECK_BOOST_VERSION],
[AC_MSG_CHECKING([for Boost version >= 1.83])
 AC_REQUIRE([QL_CHECK_BOOST_DEVEL])
 AC_COMPILE_IFELSE(
    [AC_LANG_PROGRAM(
        [[@%:@include <boost/version.hpp>]],
        [[@%:@if BOOST_VERSION < 108300
          @%:@error too old
          @%:@endif]])],
    [AC_MSG_RESULT([yes])],
    [AC_MSG_RESULT([no])
     AC_MSG_ERROR([QuantLib requires Boost 1.83 or higher])
    ])
])

# QL_CHECK_BOOST_VERSION_1_58_OR_HIGHER
# ----------------------
# Check whether the Boost installation is version 1.58
AC_DEFUN([QL_CHECK_BOOST_VERSION_1_58_OR_HIGHER],
[AC_MSG_CHECKING([for Boost version >= 1.58])
 AC_REQUIRE([QL_CHECK_BOOST_DEVEL])
 AC_COMPILE_IFELSE(
    [AC_LANG_PROGRAM(
        [[@%:@include <boost/version.hpp>]],
        [[@%:@if BOOST_VERSION < 105800
          @%:@error too old
          @%:@endif]])],
    [AC_MSG_RESULT([yes])],
    [AC_MSG_RESULT([no])
     AC_MSG_ERROR([Boost version 1.58 or higher is required for the thread-safe observer pattern])
    ])
])

# QL_CHECK_BOOST_VERSION_1_59_OR_HIGHER
# ----------------------
# Check whether the Boost installation is version 1.59
AC_DEFUN([QL_CHECK_BOOST_VERSION_1_59_OR_HIGHER],
[AC_MSG_CHECKING([for Boost version >= 1.59])
 AC_REQUIRE([QL_CHECK_BOOST_DEVEL])
 AC_COMPILE_IFELSE(
    [AC_LANG_PROGRAM(
        [[@%:@include <boost/version.hpp>]],
        [[@%:@if BOOST_VERSION < 105900
          @%:@error too old
          @%:@endif]])],
    [AC_MSG_RESULT([yes])],
    [AC_MSG_RESULT([no])
     AC_MSG_ERROR([Boost version 1.59 or higher is required for the parallel unit test runner.])
    ])
])

# QL_CHECK_BOOST_UNIT_TEST
# ------------------------
# Check whether the Boost unit-test framework is available
AC_DEFUN([QL_CHECK_BOOST_UNIT_TEST],
[AC_MSG_CHECKING([for Boost.Test])
 AC_REQUIRE([AC_PROG_CC])
 # static version
 boost_unit_found=no
 AC_LINK_IFELSE([AC_LANG_SOURCE(
     [@%:@include <boost/test/included/unit_test.hpp>
      using namespace boost::unit_test_framework;
      test_suite*
      init_unit_test_suite(int argc, char** argv)
      {
          return (test_suite*) 0;
      }
     ])],
     [boost_unit_found=yes
      boost_defines=$test_flags
      break],
     [])
 if test "$boost_unit_found" = no ; then
     AC_MSG_RESULT([no])
     AC_SUBST([HAVE_BOOST_TEST],[""])
     AC_MSG_WARN([Boost unit-test framework not found.])
     AC_MSG_WARN([The test suite will be disabled.])
 else
     AC_MSG_RESULT([yes])
     AC_SUBST([HAVE_BOOST_TEST],["yes"])
 fi
])

# QL_CHECK_BOOST_SIGNALS2
# ------------------------
# Check whether Boost signals2 is available
AC_DEFUN([QL_CHECK_BOOST_SIGNALS2],
[AC_MSG_CHECKING([whether Boost signals2 is available])
 AC_REQUIRE([AC_PROG_CC])
 ql_original_LIBS=$LIBS
 ql_original_CXXFLAGS=$CXXFLAGS
 LIBS="$ql_original_LIBS -pthread"
 CXXFLAGS="$ql_original_CXXFLAGS -pthread"
 boost_signals2_found=no
 AC_LINK_IFELSE([AC_LANG_SOURCE(
     [@%:@include <boost/signals2/signal.hpp>

      int main() {
        boost::signals2::signal<void()> sig;
      }
     ])],
     [boost_signals2_found=yes
      break],
     [])
 LIBS="$ql_original_LIBS"
 CXXFLAGS="$ql_original_CXXFLAGS"
     
 if test "$boost_signals2_found" = no ; then
     AC_MSG_RESULT([no])
     AC_SUBST([PTHREAD_LIB],[""])
     AC_MSG_ERROR([Boost signals2 library not found.
         This library is required by the thread-safe observer pattern.])
 else
     AC_MSG_RESULT([yes])
     AC_SUBST([PTHREAD_LIB],["-pthread"])
     AC_SUBST([PTHREAD_CXXFLAGS],["-pthread"])
     AC_SUBST([CXXFLAGS],["${CXXFLAGS} -pthread"])
 fi
])

# QL_CHECK_BOOST_TEST_INTERPROCESS
# ------------------------
# Check whether the Boost interprocess is available
AC_DEFUN([QL_CHECK_BOOST_TEST_INTERPROCESS],
[AC_MSG_CHECKING([whether Boost interprocess is available])
 AC_REQUIRE([AC_PROG_CC])
 ql_original_LIBS=$LIBS
 for boost_interprocess_lib in "-pthread" "-pthread -lrt"; do
     LIBS="$ql_original_LIBS $boost_interprocess_lib"
     boost_interprocess_found=no
        
	 AC_LINK_IFELSE([AC_LANG_SOURCE(
         [@%:@include <boost/interprocess/ipc/message_queue.hpp>
 
          using namespace boost::interprocess;
          int main() {
            message_queue mq(open_or_create,"message_queue",100,100);
            message_queue::remove("message_queue");
         
            return 0;
          }
         ])],
         [boost_interprocess_found=$boost_interprocess_lib
          break],
         [])
 done
 LIBS="$ql_original_LIBS"
     
 if test "$boost_interprocess_found" = no ; then
     AC_MSG_RESULT([no])
     AC_SUBST([BOOST_INTERPROCESS_LIB],[""])
     AC_MSG_ERROR([The boost library interprocess does not work on this system 
         but is required to use the parallel unit test runner.])
 else
     AC_MSG_RESULT([yes])
     AC_SUBST([BOOST_INTERPROCESS_LIB],[$boost_interprocess_lib])
     AC_SUBST([PTHREAD_CXXFLAGS],["-pthread"])
     AC_SUBST([CXXFLAGS],["${CXXFLAGS} -pthread"])
 fi
])
     

# QL_CHECK_BOOST
# ------------------------
# Boost-related tests
AC_DEFUN([QL_CHECK_BOOST],
[AC_REQUIRE([QL_CHECK_BOOST_DEVEL])
 AC_REQUIRE([QL_CHECK_BOOST_VERSION])
 AC_REQUIRE([QL_CHECK_BOOST_UNIT_TEST])
])

