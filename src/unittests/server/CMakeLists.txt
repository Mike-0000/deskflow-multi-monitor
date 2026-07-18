# SPDX-FileCopyrightText: (C) 2025 Deskflow Developers
# SPDX-License-Identifier: MIT

if(WIN32)
  set(extra_libs version app mt net)
endif()

create_test(
  NAME ServerConfigTests
  DEPENDS server
  LIBS base arch ${extra_libs}
  SOURCE ServerConfigTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/server"
)

create_test(
  NAME ServerTests
  DEPENDS server
  LIBS base arch ${extra_libs}
  SOURCE ServerTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/server"
)

create_test(
  NAME DisplayLayoutTests
  DEPENDS server
  LIBS base arch ${extra_libs}
  SOURCE DisplayLayoutTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/server"
)

create_test(
  NAME DisplayLayoutConfigTests
  DEPENDS server
  LIBS base arch ${extra_libs}
  SOURCE DisplayLayoutConfigTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/server"
)

create_test(
  NAME SwitchDecisionTests
  DEPENDS server
  LIBS base arch ${extra_libs}
  SOURCE SwitchDecisionTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/server"
)

create_test(
  NAME EdgeGraphTests
  DEPENDS server
  LIBS base arch ${extra_libs}
  SOURCE EdgeGraphTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/server"
)

create_test(
  NAME SwitchEngineTests
  DEPENDS server
  LIBS base arch ${extra_libs}
  SOURCE SwitchEngineTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/server"
)

create_test(
  NAME PoseMergeTests
  DEPENDS server
  LIBS base arch ${extra_libs}
  SOURCE PoseMergeTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/server"
)

