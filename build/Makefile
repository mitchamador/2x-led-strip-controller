MP_PROCESSOR_OPTION := attiny13
MCU := attiny13

CC=${AVR-GCC-TOOLCHAIN}/bin/avr-gcc
AVR_OBJCOPY=${AVR-GCC-TOOLCHAIN}/bin/avr-objcopy
AVR_OBJDUMP=${AVR-GCC-TOOLCHAIN}/bin/avr-objdump
AVR_SIZE=${AVR-GCC-TOOLCHAIN}/bin/avr-size
TARGET=${MP_PROCESSOR_OPTION}${TARGET_SUFFIX}

# Environment
MKDIR=mkdir -p
RM=rm -f 
MV=mv 
CP=cp  

OUTPUTNAME=2x-led-strip-controller
DEBUGGABLE_SUFFIX=elf
OUTPUT_SUFFIX=hex
ifeq ($(DEBUG),1)
IMAGE_TYPE=debug
else
IMAGE_TYPE=production
endif
OBJECTDIR=${TARGET}
DISTDIR=../dist/${TARGET}
OUTDIR=../firmware

SRCS = src/main.c
OBJS = $(SRCS:%.c=${OBJECTDIR}/${IMAGE_TYPE}/%.o)
DEPS := $(OBJS:.o=.d)

VPATH = ..

-include $(DEPS)

.PHONY: usage build clean

usage:
	$(info usage:)
	$(info make AVR-GCC-TOOLCHAIN=/path/to/avr-gcc-toolchain)

build: $(TARGET)
	${CP} ${DISTDIR}/${IMAGE_TYPE}/${OUTPUTNAME}.production.${OUTPUT_SUFFIX} ${OUTDIR}/${OUTPUTNAME}.${MCU}${TARGET_SUFFIX}.${OUTPUT_SUFFIX}
	${CP} ${DISTDIR}/${IMAGE_TYPE}/${OUTPUTNAME}.production.eep ${OUTDIR}/${OUTPUTNAME}.${MCU}${TARGET_SUFFIX}.eep

$(TARGET): $(OBJS)
	@${MKDIR} ${DISTDIR}/${IMAGE_TYPE} 
	${CC} $(LDFLAGS) -gdwarf-2 -Wl,-Map="${DISTDIR}/${IMAGE_TYPE}/${OUTPUTNAME}.production.map" -o ${DISTDIR}/${IMAGE_TYPE}/${OUTPUTNAME}.production.${DEBUGGABLE_SUFFIX} -mmcu=$(MCU) -Os -Wl,--gc-sections -flto -fuse-linker-plugin ${OBJS} 
	${AVR_OBJCOPY} -O ihex -R .eeprom "${DISTDIR}/${IMAGE_TYPE}/${OUTPUTNAME}.production.${DEBUGGABLE_SUFFIX}" "${DISTDIR}/${IMAGE_TYPE}/${OUTPUTNAME}.production.${OUTPUT_SUFFIX}"
	${AVR_OBJCOPY} -j .eeprom --set-section-flags=.eeprom=alloc,load --change-section-lma .eeprom=0 --no-change-warnings -O ihex "${DISTDIR}/${IMAGE_TYPE}/${OUTPUTNAME}.production.${DEBUGGABLE_SUFFIX}" "${DISTDIR}/${IMAGE_TYPE}/${OUTPUTNAME}.production.eep" || exit 0 
	${AVR_OBJDUMP} -h -S "${DISTDIR}/${IMAGE_TYPE}/${OUTPUTNAME}.production.${DEBUGGABLE_SUFFIX}" >"${DISTDIR}/${IMAGE_TYPE}/${OUTPUTNAME}.production.lss"
	${AVR_SIZE} -C --mcu=$(MCU) "${DISTDIR}/${IMAGE_TYPE}/${OUTPUTNAME}.production.${DEBUGGABLE_SUFFIX}"

${OBJECTDIR}/${IMAGE_TYPE}/%.o : %.c
	@${MKDIR} "${OBJECTDIR}/${IMAGE_TYPE}/src"
ifeq ($(DEBUG),1)
	${CC} $(CPPFLAGS) -o $@ -mmcu=$(MCU) -std=gnu11 -fno-fat-lto-objects -MD -Og -D_DEBUG_ -ffunction-sections -fdata-sections -flto -funsigned-char -funsigned-bitfields -fpack-struct -fshort-enums $(MP_EXTRA_CC_PRE) -I../include -I../src -c $<
else
	${CC} $(CPPFLAGS) -o $@ -mmcu=$(MCU) -std=gnu11 -fno-fat-lto-objects -MD -Os -ffunction-sections -fdata-sections -flto -funsigned-char -funsigned-bitfields -fpack-struct -fshort-enums -mcall-prologues $(MP_EXTRA_CC_PRE) -I../include -I../src -c $<
endif

clean:
	@rm -rf $(OBJECTDIR)
	@rm -rf $(DISTDIR)
