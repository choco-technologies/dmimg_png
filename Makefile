# #############################################################################
# 
# 	dmimg_png - the PNG decoder of dmimg (a library module).
#
# #############################################################################
DMOD_DIR=@DMOD_DIR@

# -----------------------------------------------------------------------------
#  Paths initialization
# -----------------------------------------------------------------------------
include $(DMOD_DIR)/paths.mk

# -----------------------------------------------------------------------------
#   Module configuration
# -----------------------------------------------------------------------------

# The name of the module
DMOD_MODULE_NAME=dmimg_png

# The version of the module
DMOD_MODULE_VERSION=0.1

# The name of the author
DMOD_AUTHOR_NAME=Patryk Kubiak

# The list of C sources
DMOD_CSOURCES=src/dmimg_png.c third_party/pngle/pngle.c third_party/pngle/miniz.c

# The list of C++ sources
DMOD_CXXSOURCES=

# The list of include directories
DMOD_INC_DIRS=third_party/pngle

# The list of libraries to link
DMOD_LIBS=

# The list of definitions
DMOD_DEFINITIONS=PNGLE_NO_GAMMA_CORRECTION MINIZ_NO_MALLOC calloc=dmimg_png_calloc free=dmimg_png_free abs=dmimg_png_abs

# -----------------------------------------------------------------------------
#   List of MAL interfaces implemented by the module
# -----------------------------------------------------------------------------
DMOD_MAL_IMPLS=

# -----------------------------------------------------------------------------
#   Include the dmod app makefile
# -----------------------------------------------------------------------------
include $(DMOD_DMF_LIB_FILE_PATH)
