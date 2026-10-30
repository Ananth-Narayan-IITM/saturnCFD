if(NOT SATURN_BUILD_DOCS)
  return()
endif()

find_package(Doxygen QUIET)

if(DOXYGEN_FOUND)
  add_custom_target(docHtml
    COMMAND ${CMAKE_COMMAND} -E make_directory ${SATURN_SOURCE_DIR}/build
    COMMAND ${DOXYGEN_EXECUTABLE} ${SATURN_SOURCE_DIR}/docs/Doxyfile
    WORKING_DIRECTORY ${SATURN_SOURCE_DIR}
    COMMENT "Generating Doxygen HTML and LaTeX"
    VERBATIM)

  find_program(SATURN_LATEXMK latexmk)
  if(SATURN_LATEXMK)
    add_custom_target(docPdf
      COMMAND ${SATURN_LATEXMK} -pdf refman.tex
      WORKING_DIRECTORY ${SATURN_SOURCE_DIR}/build/doxygen/latex
      COMMENT "Building Doxygen PDF"
      VERBATIM)
    add_dependencies(docPdf docHtml)
  else()
    message(STATUS "saturnCFD: latexmk not found; docPdf target disabled")
  endif()

  add_custom_target(doc DEPENDS docHtml)
  if(TARGET docPdf)
    add_dependencies(doc docPdf)
  endif()
else()
  message(STATUS "saturnCFD: Doxygen not found; doc targets disabled")
endif()

find_program(SATURN_CLANG_FORMAT clang-format)
if(SATURN_CLANG_FORMAT)
  file(GLOB_RECURSE SATURN_FORMAT_FILES CONFIGURE_DEPENDS
    ${SATURN_SOURCE_DIR}/src/*.hpp
    ${SATURN_SOURCE_DIR}/src/*.cpp
    ${SATURN_SOURCE_DIR}/apps/*.hpp
    ${SATURN_SOURCE_DIR}/apps/*.cpp
    ${SATURN_SOURCE_DIR}/tests/*.hpp
    ${SATURN_SOURCE_DIR}/tests/*.cpp
    ${SATURN_SOURCE_DIR}/tools/*.hpp
    ${SATURN_SOURCE_DIR}/tools/*.cpp)
  add_custom_target(formatCode
    COMMAND ${SATURN_CLANG_FORMAT} -i ${SATURN_FORMAT_FILES}
    COMMENT "Running clang-format"
    VERBATIM)
else()
  message(STATUS "saturnCFD: clang-format not found; format target disabled")
endif()
