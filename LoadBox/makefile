
	# check on which OS we build
	UNAME_S := $(shell uname -s)
	# check which architecture we build on
	UNAME_M := $(shell uname -m)
	# check which architecture we build for
	TARGET_ARCH = UNKNOWN
	# check if user is root
	user = $(shell whoami)
	ifeq ($(user),root)
	CLAP_INSTAL_DIR = /usr/lib/clap
	VST2_INSTAL_DIR = /usr/lib/vst
	VST3_INSTALL_DIR = /usr/lib/vst3
	else 
	CLAP_INSTAL_DIR = ~/.clap
	VST2_INSTAL_DIR = ~/.vst
	VST3_INSTALL_DIR = ~/.vst3
	endif


	# check which architecture we build for
	TARGET_ARCH = $(shell $(CXX) -dumpmachine | sed 's/-.*//')

ifneq ($(UNAME_M),$(TARGET_ARCH))
  CROSS_COMPILING = 1
  $(info $(yellow) INFO: $(reset)Cross Compile $(blue)$(UNAME_M)$(reset) to $(blue)$(TARGET_ARCH)$(reset))
endif

# avoid optimisation for x86_64 arch when we cross compile
ifneq ($(CROSS_COMPILING), 1)

# get flags supported by CPU
ifeq ($(UNAME_S), FreeBSD)
  CPU_INFO = dmesg | grep Features | tr A-Z a-z
  ifeq ($(UNAME_M), amd64)
    UNAME_M = x86_64
  endif
else
  CPU_INFO = cat /proc/cpuinfo | grep flags
endif

# check for sse optimisation level only on x86_64 architecture
ifeq ($(TARGET_ARCH), x86_64)
  ifneq ($$(filter $(CPU_INFO) | grep sse3 ) , )
    SSE_CFLAGS = -msse3 -mfpmath=sse -mfxsr -DUSE_SSE=1
    FFT_FLAG = -DFFTCONVOLVER_USE_SSE=1
  else ifneq ($$(filter $(CPU_INFO) | grep sse2 ) , )
    SSE_CFLAGS = -msse2 -mfpmath=sse -mfxsr -DUSE_SSE=1
    FFT_FLAG = -DFFTCONVOLVER_USE_SSE=1
  else ifneq ($$(filter $(CPU_INFO) | grep sse ) , )
    SSE_CFLAGS = -msse -mfpmath=sse -mfxsr -DUSE_SSE=1
    FFT_FLAG = -DFFTCONVOLVER_USE_SSE=1
  else ifneq ($$(filter $(CPU_INFO) | grep ARM ) , )
    ifneq ($$(filter $(CPU_INFO) | grep ARMv7 ) , )
      ifneq ($$(filter $(CPU_INFO) | grep vfpd32 ) , )
        SSE_CFLAGS = -march=armv7-a -mfpu=vfpv3 
      else ifneq ($$(filter $(CPU_INFO) | grep vfpv3 ) , )
        SSE_CFLAGS = -march=armv7-a -mfpu=vfpv3
      endif
    else
      ARMCPU = "YES"
    endif
  endif
else
  SSE_CFLAGS =
endif

# check for x86_64 optimisation level
FLAGS_v3    := avx avx2 bmi1 bmi2 f16c fma abm movbe xsave
FLAGS_v2    := cx16 lahf_lm popcnt sse4_1 sse4_2 ssse3
FLAGS      := $(shell $(CPU_INFO))


NO_x86_64 =
define LOOPBODY
  ifneq ($$(filter $(v),$(FLAGS)),)
    YES_x86_64 = $(x86_64_LEVEL)
  else
    NO_x86_64 = $(x86_64_LEVEL)
  endif
endef

x86_64_LEVEL := -v3
$(foreach v,$(FLAGS_v3),$(eval $(LOOPBODY)))
ifeq ($(NO_x86_64), $(x86_64_LEVEL))
  x86_64_LEVEL := -v2
  $(foreach v,$(FLAGS_v2),$(eval $(LOOPBODY)))
endif
ifeq ($(NO_x86_64), $(x86_64_LEVEL))
  x86_64_LEVEL :=
endif

endif

CXX_v = $(shell $(CXX) -dumpversion)
CXX_VERSION   := $(subst ., ,$(lastword $(CXX_v)))
CXX_MAJOR_VER := $(word 1,$(CXX_VERSION))
ifneq ($(MAKECMDGOALS),install)
  ifeq (,$(filter uninstall,$(MAKECMDGOALS)))
    ifneq ($(MAKECMDGOALS),clean)
      ifneq ($(NO_x86_64), $(x86_64_LEVEL))
        ifeq ($(TARGET_ARCH), aarch64)
          CXXFLAGS += -march=armv8-a
          $(info $(yellow) INFO: $(reset)optimised for $(blue)-march=armv8-a $(SSE_CFLAGS)$(reset))
        else ifeq ($(shell awk -v a="$(CXX_MAJOR_VER)" -v b="11" 'BEGIN{print(a<b)}'), 1)
          SSE_CFLAGS += -march=native
          $(info $(yellow) INFO: $(reset)optimised for $(blue)$(SSE_CFLAGS)$(reset))
        else ifeq ($(TARGET_ARCH), x86_64)
          SSE_CFLAGS += -march=x86-64$(x86_64_LEVEL)
          $(info $(yellow) INFO: $(reset)optimised for $(blue)$(SSE_CFLAGS)$(reset))
        else
          SSE_CFLAGS += -march=native
          $(info $(yellow) INFO: $(reset)optimised for $(blue)$(SSE_CFLAGS)$(reset))
        endif
      endif
    endif
  endif
endif

# check for c++ level
ifeq (,$(filter clean,$(MAKECMDGOALS)))
  ifeq (,$(filter install,$(MAKECMDGOALS)))
    ifeq (,$(filter uninstall,$(MAKECMDGOALS)))
      $(info $(yellow) INFO: $(reset)build with    $(blue)$(CXX) $(CXX_v)$(reset))
      ifeq ($(shell awk -v a="$(CXX_MAJOR_VER)" -v b="11" 'BEGIN{print(a<b)}'), 1)
        CXXFLAGS += -std=c++17
        $(info $(yellow) INFO: $(reset)using         $(blue)-std=c++17$(reset))
      else
        CXXFLAGS += -std=c++20
        $(info $(yellow) INFO: $(reset)using         $(blue)-std=c++20$(reset))
    endif
    endif
  endif
endif

ifneq (,$(filter debug,$(MAKECMDGOALS)))
  CXXFLAGS += -g -O0
  CFLAGS += -g -O0
  DEBUG = yes
    else
  CXXFLAGS += -O3
  CFLAGS += -02
  DEBUG = no
endif


	NAME = LoadBox
	space := $(subst ,, )
	VST3_NAME := LoadBoxVST3
	LIB_DIR := ../libxputty/libxputty/
	HEADER_DIR := $(LIB_DIR)include/
	UI_LIB:= $(LIB_DIR)libxputty.$(STATIC_LIB_EXT)
	STRIP ?= strip

	PREFIX ?= /usr
	SHARE_DIR ?= $(PREFIX)/share

	NEURALAUDIO_DIR := ../NeuralAudio/NeuralAudio/
	NEURALAUDIO_DEP_DIR := ../NeuralAudio/deps/
	NEURALAUDIO_SOURCES := $(wildcard $(NEURALAUDIO_DIR)*.cpp)
	NEURALAUDIO_SOURCES += $(wildcard $(NEURALAUDIO_DEP_DIR)RTNeural/RTNeural/*.cpp)
	NEURALAUDIO_SOURCES += $(wildcard $(NEURALAUDIO_DEP_DIR)NeuralAmpModelerCore/NAM/*.cpp)
	NEURALAUDIO_SOURCES += $(wildcard $(NEURALAUDIO_DEP_DIR)NeuralAmpModelerCore/NAM/wavenet/*.cpp)
	NEURALAUDIO_OBJ := $(patsubst %.cpp,%.o,$(NEURALAUDIO_SOURCES))
	NEURALAUDIO_INCLUDES := -I$(NEURALAUDIO_DIR) -I$(NEURALAUDIO_DEP_DIR)RTNeural/ -I./ \
	-I$(NEURALAUDIO_DEP_DIR)math_approx/include/ -I$(NEURALAUDIO_DEP_DIR) \
	-I$(NEURALAUDIO_DEP_DIR)NeuralAmpModelerCore/Dependencies/nlohmann/ \
	-I$(NEURALAUDIO_DEP_DIR)NeuralAmpModelerCore/Dependencies/eigen \
	-I$(NEURALAUDIO_DEP_DIR)NeuralAmpModelerCore -I../NeuralAudio/
	NEURALAUDIO_LIB := libneuralaudio.$(STATIC_LIB_EXT)

	ENGINE_DIR := ./engine/
	ENGINE_INCLUDE := -I./engine/

	MODEL_LOADER := $(ENGINE_DIR)NeuralModelLoader.cpp

	GUI_DIR := ./gui/
	GUI_INCLUDE := -I./gui/

	WRAPPER_DIR := ./wrapper/

	CLAP_DIR := ./clap/
	CLAP_INCLUDE := -I./engine/ -I./wrapper/clap/clap/ -I./clap/ -I./wrapper/ -I./wrapper/clap/ -I./plugdef/ -I./gui/
	CLAP_SOURCES := $(CLAP_DIR)LoadBoxClap.cpp

	VST2_DIR := ./vst2/
	VST2_INCLUDE := -I./engine/ -I./wrapper/ -I./wrapper/vst2/ -I./vst2/ -I./plugdef/ -I./gui/
	VST2_SOURCES := $(VST2_DIR)LoadBoxvst.cpp

	VST3_DIR := ./vst3/
	VST3_INCLUDE := -I./engine/ -I./vst3/ -I./wrapper/vst3/travesty/ -I./wrapper/ -I./wrapper/vst3/ -I./plugdef/ -I./gui/
	VST3_SOURCES := $(VST3_DIR)LoadBoxVST3.cpp
	ARCH = `echo $(UNAME_S) | tr A-Z a-z`
	VST3_BUNDLE := $(VST3_NAME).vst3/Contents/$(UNAME_M)-$(ARCH)/

	CONV_DIR := ../FFTConvolver/
	CONV_SOURCES :=  $(wildcard $(CONV_DIR)*.cpp)
	CONV_SOURCES += ./engine/fftconvolver.cpp
	CONV_OBJ := $(patsubst %.cpp,%.o,$(CONV_SOURCES))
	CONV_LIB := libfftconvolver.$(STATIC_LIB_EXT)

	RESAMP_DIR := ./zita-resampler-1.1.0/
	RESAMP_SOURCES := $(wildcard $(RESAMP_DIR)*.cc) $(ENGINE_DIR)gx_resampler.cc
	RESAMP_OBJ := $(patsubst %.cc,%.o,$(RESAMP_SOURCES))
	RESAMP_LIB := libzita-resampler.$(STATIC_LIB_EXT)

	DEPS = $(NEURALAUDIO_OBJ:%.o=%.d) $(CONV_OBJ:%.o=%.d) $(RESAMP_OBJ:%.o=%.d) LoadBox_vst2.d LoadBox_vst3.d LoadBox_clap.d

ifeq ($(PAWPAW_BUILD),1)
	CXXFLAGS += -DPAWPAW=1
endif

ifeq ($(TARGET), Linux)

	GUI_LDFLAGS += -I$(HEADER_DIR) $(GUI_INCLUDE) -Wl,-Bstatic -L. $(UI_LIB) \
	-Wl,-Bdynamic `pkg-config --cflags --libs cairo x11` $(FT_LIBS)

	LDFLAGS += -fvisibility=hidden -shared -lm -fPIC -pthread -lpthread \
	-Wl,-z,noexecstack -Wl,--no-undefined -Wl,--gc-sections  -Wl,--exclude-libs,ALL \
	`$(PKGCONFIG) --cflags --libs sndfile`

	CXXFLAGS += -flto=auto -fPIC -DPIC -Wall -funroll-loops $(SSE_CFLAGS) \
	-Wno-sign-compare -Wno-reorder -Wno-infinite-recursion -DUSE_ATOM $(FFT_FLAG) \
	-fomit-frame-pointer -fstack-protector -fvisibility=hidden -Wno-pessimizing-move \
	-fdata-sections -I. -I./ -I./zita-resampler-1.1.0 -I$(CONV_DIR) -DNDEBUG \
	-DRTNEURAL_DEFAULT_ALIGNMENT=32 -DRTNEURAL_USE_EIGEN=1 -DRTNEURAL_NAMESPACE=RTNeural \
	-DDSP_SAMPLE_FLOAT -DNAM_SAMPLE_FLOAT -Dneural_amp_modeler_EXPORTS

	ifneq ($(UNAME_S), FreeBSD)
	ifeq (,$(findstring clang, $(CXX)))
	CXXFLAGS += -fstrength-reduce -fno-fat-lto-objects -Wno-deprecated-declarations
	else
	CXXFLAGS += -Wno-unused-private-field -fdenormal-fp-math=positive-zero -Wno-vla-cxx-extension \
	-Wno-unknown-argument -Wno-nan-infinity-disabled
	endif
	else
	CXXFLAGS += -Wno-unused-private-field -fdenormal-fp-math=positive-zero \
	-Wno-vla-cxx-extension -Wno-nan-infinity-disabled
	endif

	# remove unwanted flags from PawPaw
	ifneq (,$(findstring clang, $(CXX)))
	CXXFLAGS := $(filter-out -fno-gnu-unique,$(CXXFLAGS))
	LDFLAGS := $(filter-out -fno-gnu-unique,$(LDFLAGS))
	endif

	CFLAGS := -D_FORTIFY_SOURCE=2 -Wall -Wno-unused-result -fstack-protector -fvisibility=hidden \
	-fdata-sections -Wl,--gc-sections -Wl,-z,relro,-z,now -Wl,--exclude-libs,ALL -DUSE_ATOM

else ifeq ($(TARGET), Windows)
	VST3_BUNDLE := $(VST3_NAME).vst3/Contents/$(UNAME_M)-win/

	CXXFLAGS += -D_FORTIFY_SOURCE=2 -I. -fPIC -DPIC -Wall -funroll-loops \
	-ffast-math -fomit-frame-pointer -fstrength-reduce -Wno-deprecated-declarations \
	-Wno-maybe-uninitialized -Wno-uninitialized \
	-Wno-sign-compare -Wno-reorder -Wno-infinite-recursion -DUSE_ATOM $(SSE_CFLAGS) $(FFT_FLAG) \
	-fdata-sections -I./ -I./zita-resampler-1.1.0 -I$(CONV_DIR) -DNDEBUG \
	-DRTNEURAL_DEFAULT_ALIGNMENT=32 -DRTNEURAL_USE_EIGEN=1 -DRTNEURAL_NAMESPACE=RTNeural \
	-DDSP_SAMPLE_FLOAT -DNAM_SAMPLE_FLOAT -Dneural_amp_modeler_EXPORTS

	LDFLAGS += -I. -shared -lm $(PAWPAW_LFLAGS) -Wl,--gc-sections \
	-Wl,--exclude-libs,ALL `$(PKGCONFIG) --cflags --libs sndfile`

	GUI_LDFLAGS += -I$(HEADER_DIR) $(GUI_INCLUDE) -static-libgcc -static-libstdc++ \
	`$(PKGCONFIG) $(PKGCONFIG_FLAGS) --cflags --libs cairo ` \
	-L. $(LIB_DIR)libxputty.$(STATIC_LIB_EXT) -lm $(PAWPAW_LFLAGS) $(FT_LIBS)

	CFLAGS := -D_FORTIFY_SOURCE=2 -Wall -fstack-protector -fvisibility=hidden \
	-fdata-sections -Wl,--gc-sections -Wl,--exclude-libs,ALL -DUSE_ATOM

endif

ifneq ($(MAKECMDGOALS),install)
  ifneq ($(MAKECMDGOALS),uninstall)
    ifneq ($(MAKECMDGOALS),clean)
      $(info ==================$(yellow) CXXFLAGS $(reset)=====================)
      $(info $(blue)$(CXXFLAGS) $(reset))
      $(info ==================$(yellow) LDFLAGS $(reset)=====================)
      $(info $(blue)$(LDFLAGS) $(reset))
      $(info ===================$(yellow) BUILD $(reset)=======================)
    endif
  endif
endif

.PHONY : all install uninstall clean

.NOTPARALLEL:

all: clap vst2 vst3

debug: all

vst2: $(NAME)vst.$(LIB_EXT)
	$(QUIET)mkdir -p ../bin/
	$(QUIET)cp ./$(NAME)vst.$(LIB_EXT) ../bin/
	@$(B_ECHO) "=================== DONE =======================$(reset)"

vst3: $(VST3_NAME)
	@$(B_ECHO) "Create $(VST3_BUNDLE) $(reset)"
	$(QUIET)mkdir -p ../bin/
	$(QUIET)mkdir -p ../bin/$(VST3_BUNDLE)
ifeq ($(TARGET), Windows)
	$(QUIET)cp ./$(VST3_NAME).$(LIB_EXT) ../bin/$(VST3_BUNDLE)/$(VST3_NAME).vst3
else
	$(QUIET)cp ./$(VST3_NAME).$(LIB_EXT) ../bin/$(VST3_BUNDLE)
endif
	@$(B_ECHO) "=================== DONE =======================$(reset)"

clap: $(NAME).clap
	$(QUIET)mkdir -p ../bin/
	$(QUIET)cp ./$(NAME).clap ../bin/
	@$(B_ECHO) "=================== DONE =======================$(reset)"

-include $(DEPS)

$(NEURALAUDIO_OBJ): $(NEURALAUDIO_SOURCES)
	@$(ECHO) "Building object file $@ $(reset)"
	$(QUIET)$(CXX) $(CXXFLAGS) -DWAVENET_FRAMES=128 -DBUILD_NAMCORE -MMD -Wall -Wno-unused-private-field -Wno-reorder-ctor -Wno-infinite-recursion -c $(patsubst %.o,%.cpp,$@) -o $@ $(NEURALAUDIO_INCLUDES)

$(NEURALAUDIO_LIB): $(NEURALAUDIO_OBJ)
	@$(B_ECHO) "Build static library $@ $(reset)"
	$(QUIET)$(AR) rcs $(NEURALAUDIO_LIB) $(NEURALAUDIO_OBJ)
	@$(B_ECHO) "=================== DONE =======================$(reset)"

$(CONV_OBJ): $(CONV_SOURCES)
	@$(ECHO) "Building object file $@ $(reset)"
	$(QUIET)$(CXX) $(CXXFLAGS) -MMD -Wall -c $(patsubst %.o,%.cpp,$@) -o $@

$(CONV_LIB): $(CONV_OBJ)
	@$(B_ECHO) "Build static library $@ $(reset)"
	$(QUIET)$(AR) rcs $(CONV_LIB) $(CONV_OBJ)
	@$(B_ECHO) "=================== DONE =======================$(reset)"

$(RESAMP_OBJ): $(RESAMP_SOURCES)
	@$(ECHO) "Building object file $@ $(reset)"
	$(QUIET)$(CXX) $(CXXFLAGS) -MMD  -c $(patsubst %.o,%.cc,$@) -o $@ -I./zita-resampler-1.1.0 $(ENGINE_INCLUDE)

$(RESAMP_LIB): $(RESAMP_OBJ)
	@$(B_ECHO) "Build static library $@ $(reset)"
	$(QUIET)$(AR) rcs $(RESAMP_LIB) $(RESAMP_OBJ)
	@$(B_ECHO) "=================== DONE =======================$(reset)"

$(NAME)vst.$(LIB_EXT): $(VST2_SOURCES) ./plugdef/PluginClient.cc ./plugdef/LoadBox.cc $(NEURALAUDIO_LIB) $(CONV_LIB) $(RESAMP_LIB) $(MODEL_LOADER)
	@$(B_ECHO) "Compiling $(NAME)vst.$(LIB_EXT) $(reset)"
	$(QUIET)$(CXX) $(CXXFLAGS) -MMD -MF $(NAME)_vst2.d -Wno-multichar $(NEURALAUDIO_INCLUDES) $(ENGINE_INCLUDE) $(MODEL_LOADER) $(VST2_INCLUDE) $(VST2_SOURCES)  \
	-L. -Wl,--whole-archive $(NEURALAUDIO_LIB) -Wl,--no-whole-archive -L. $(CONV_LIB) -L. $(RESAMP_LIB) $(GUI_LDFLAGS) $(LDFLAGS) -o $(NAME)vst.$(LIB_EXT)
ifeq (,$(filter yes,$(DEBUG)))
	$(QUIET)$(STRIP) -s -x -X -R .comment -R .note.ABI-tag $(NAME)vst.$(LIB_EXT)
endif

$(VST3_NAME): $(VST3_SOURCES) ./plugdef/PluginClient.cc ./plugdef/LoadBox.cc $(NEURALAUDIO_LIB) $(CONV_LIB) $(RESAMP_LIB) $(MODEL_LOADER)
	@$(B_ECHO) "Compiling $(VST3_NAME).$(LIB_EXT) $(reset)"
	$(QUIET)$(CXX) $(CXXFLAGS) -MMD -MF $(NAME)_vst3.d -Wno-multichar $(NEURALAUDIO_INCLUDES) $(ENGINE_INCLUDE) $(MODEL_LOADER) $(VST3_INCLUDE) $(VST3_SOURCES)  \
	-L. -Wl,--whole-archive $(NEURALAUDIO_LIB) -Wl,--no-whole-archive -L. $(CONV_LIB) -L. $(RESAMP_LIB) $(GUI_LDFLAGS) $(LDFLAGS) -o $(VST3_NAME).$(LIB_EXT)
ifeq (,$(filter yes,$(DEBUG)))
	$(QUIET)$(STRIP) -s -x -X -R .comment -R .note.ABI-tag $(VST3_NAME).$(LIB_EXT)
endif

$(NAME).clap: $(CLAP_SOURCES) ./plugdef/PluginClient.cc ./plugdef/LoadBox.cc $(NEURALAUDIO_LIB) $(CONV_LIB) $(RESAMP_LIB) $(MODEL_LOADER)
	@$(B_ECHO) "Compiling $(NAME).clap $(reset)"
	$(QUIET)$(CXX) $(CXXFLAGS) -MMD -MF $(NAME)_clap.d $(NEURALAUDIO_INCLUDES) $(ENGINE_INCLUDE) $(MODEL_LOADER) $(CLAP_INCLUDE) $(CLAP_SOURCES)  \
	-L. -Wl,--whole-archive $(NEURALAUDIO_LIB) -Wl,--no-whole-archive -L. $(CONV_LIB) -L. $(RESAMP_LIB) $(GUI_LDFLAGS) $(LDFLAGS) -o $(NAME).clap
ifeq (,$(filter yes,$(DEBUG)))
	$(QUIET)$(STRIP) -s -x -X -R .comment -R .note.ABI-tag $(NAME).clap
endif

install :
ifeq ($(TARGET), Linux)
ifneq ("$(wildcard ../bin/$(NAME).clap)","")
	@$(B_ECHO) "Install  $(NAME).clap to $(DESTDIR)$(CLAP_INSTAL_DIR)/$(reset)"
	$(QUIET)mkdir -p $(DESTDIR)$(CLAP_INSTAL_DIR)/
	$(QUIET)cp -r ../bin/$(NAME).clap $(DESTDIR)$(CLAP_INSTAL_DIR)/$(NAME).clap
	@$(B_ECHO) ". ., done$(reset)"
else
	@$(B_ECHO) "$(NAME).clap Clap skipped$(reset)"
endif
ifneq ("$(wildcard ../bin/$(NAME)vst.$(LIB_EXT))","")
	@$(B_ECHO) "Install  $(NAME)vst.$(LIB_EXT) to $(DESTDIR)$(VST2_INSTAL_DIR)/$(reset)"
	$(QUIET)mkdir -p $(DESTDIR)$(VST2_INSTAL_DIR)/
	$(QUIET)cp -r ../bin/$(NAME)vst.$(LIB_EXT) $(DESTDIR)$(VST2_INSTAL_DIR)/$(NAME)vst.$(LIB_EXT)
	@$(B_ECHO) ". ., done$(reset)"
else
	@$(B_ECHO) "$(NAME)vst.$(LIB_EXT) vst2 skipped$(reset)"
endif
ifneq ("$(wildcard ../bin/$(VST3_NAME).vst3)","")
	@$(B_ECHO) "Install $(VST3_NAME).vst3 to $(DESTDIR)$(VST3_INSTALL_DIR)/$(reset)"
	$(QUIET)mkdir -p $(DESTDIR)$(VST3_INSTALL_DIR)/$(VST3_NAME).vst3
	$(QUIET)cp -r ../bin/$(VST3_NAME).vst3/* $(DESTDIR)$(VST3_INSTALL_DIR)/$(VST3_NAME).vst3
	@$(B_ECHO) ". ., done$(reset)"
else
	@$(B_ECHO) "$(VST3_NAME).vst3 skipped$(reset)"
endif

else
	$(QUIET)$(R_ECHO) "Install is not implemented for windows$(reset)"
endif

uninstall :
ifeq ($(TARGET), Linux)
	@$(B_ECHO) "Uninstall $(NAME) $(reset)"
	$(QUIET)rm -rf $(DESTDIR)$(CLAP_INSTAL_DIR)/$(NAME).clap
	$(QUIET)rm -rf $(DESTDIR)$(VST2_INSTAL_DIR)/$(NAME)vst.$(LIB_EXT)
	$(QUIET)rm -rf $(DESTDIR)$(VST3_INSTALL_DIR)/$(VST3_NAME).vst3
	@$(B_ECHO) ". ., done$(reset)"
else
	@$(R_ECHO) "Uninstall is not implemented for windows$(reset)"
endif

clean:
	$(QUIET)rm -f *.a  *.lib *.o *.d *.so *.dll *.clap
	$(QUIET)rm -f $(RESAMP_DIR)*.a $(RESAMP_DIR)*.lib $(RESAMP_DIR)*.o $(RESAMP_DIR)*.d
	$(QUIET)rm -f $(CONV_DIR)*.a $(CONV_DIR)*.lib $(CONV_DIR)*.o $(CONV_DIR)*.d
	$(QUIET)rm -f $(NEURALAUDIO_DIR)*.a $(NEURALAUDIO_DIR)*.lib $(NEURALAUDIO_DIR)*.o $(NEURALAUDIO_DIR)*.d
	$(QUIET)rm -f $(NEURALAUDIO_DEP_DIR)RTNeural/RTNeural/*.a $(NEURALAUDIO_DEP_DIR)RTNeural/RTNeural/*.lib \
	$(NEURALAUDIO_DEP_DIR)RTNeural/RTNeural/*.o $(NEURALAUDIO_DEP_DIR)RTNeural/RTNeural/*.d
	$(QUIET)rm -f $(ENGINE_DIR)*.a $(ENGINE_DIR)*.lib $(ENGINE_DIR)*.o $(ENGINE_DIR)*.d
	$(QUIET)rm -rf ../bin
ifndef EXTRAQUIET
	@$(B_ECHO) "=================== DONE =======================$(reset)"
endif

