set(subdirs helpers observers runners solvers testers utils modeling)
foreach (subdir ${subdirs})
	string(SUBSTRING ${subdir} 0 1 FIRST_LETTER)
	string(TOUPPER ${FIRST_LETTER} FIRST_LETTER)
	string(REGEX REPLACE "^.(.*)" "${FIRST_LETTER}\\1" subdir_cap "${subdir}")

  source_group("EasyLocal\\${subdir_cap}" REGULAR_EXPRESSION "[Ee]asy[Ll]ocal.*/include/${subdir}/.+\.hh$")
endforeach (subdir)

source_group("EasyLocal" REGULAR_EXPRESSION "[Ee]asy[Ll]ocal.*/include/[^/]+\.hh$")

#source_group("EasyLocal" REGULAR_EXPRESSION "[Ee]asy[Ll]ocal.*/.+\.hh$")

