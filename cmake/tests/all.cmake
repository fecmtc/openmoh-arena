enable_testing()

include(tests/lz77)
include(tests/serverlist_scheduler)
include(tests/pm_cmdscale)
# Added in MoH Arena: the modern UI tests, only built with the modern UI and when asked for.
if(MOHARENA_MODERN_UI AND MOHARENA_UI_TESTS)
	include(tests/uirender)
	include(tests/uir_design)
	include(tests/uid_bench)
endif()
