require 'mkmf'

dir_config("bmh_search")
have_library("c", "main")

create_makefile("bmh_search")
