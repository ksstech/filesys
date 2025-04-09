# FILESYS

set( srcs "filesys.c" )
set( include_dirs "include" )
set( priv_include_dirs )
set( requires "hal_esp32 spi_flash" )

set( priv_requires )

idf_component_register(
	SRCS ${srcs}
	INCLUDE_DIRS ${include_dirs}
	PRIV_INCLUDE_DIRS ${priv_include_dirs}
	REQUIRES ${requires}
	PRIV_REQUIRES ${priv_requires}
)
