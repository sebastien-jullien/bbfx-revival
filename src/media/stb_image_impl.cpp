// v3.5 Lot Q — dedicated translation unit for the stb_image
// implementation. Multiple modules (SurfaceEditorPanel, SequencePlayer,
// …) just `#include <stb_image.h>` and share the single implementation
// emitted here, avoiding LNK2005 duplicate-symbol errors.

#ifdef BBFX_HAS_STB
#  define STB_IMAGE_IMPLEMENTATION
#  include <stb_image.h>
#endif
