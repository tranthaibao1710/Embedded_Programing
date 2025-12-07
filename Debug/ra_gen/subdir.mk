################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../ra_gen/common_data.c \
../ra_gen/hal_data.c \
../ra_gen/main.c \
../ra_gen/pin_data.c \
../ra_gen/vector_data.c 

C_DEPS += \
./ra_gen/common_data.d \
./ra_gen/hal_data.d \
./ra_gen/main.d \
./ra_gen/pin_data.d \
./ra_gen/vector_data.d 

CREF += \
mame_CM33.cref 

OBJS += \
./ra_gen/common_data.o \
./ra_gen/hal_data.o \
./ra_gen/main.o \
./ra_gen/pin_data.o \
./ra_gen/vector_data.o 

MAP += \
mame_CM33.map 


# Each subdirectory must supply rules for building sources it contributes
ra_gen/%.o: ../ra_gen/%.c
	@echo 'Building file: $<'
	$(file > $@.in,-mcpu=cortex-m33 -mthumb -mlittle-endian -mfloat-abi=hard -mfpu=fpv5-sp-d16 -O2 -ffunction-sections -fdata-sections -fno-strict-aliasing -fmessage-length=0 -funsigned-char -Wunused -Wuninitialized -Wall -Wextra -Wmissing-declarations -Wconversion -Wpointer-arith -Wshadow -Waggregate-return -Wno-parentheses-equality -Wfloat-equal -g3 -std=c99 -fshort-enums -fno-unroll-loops -I"C:\\Users\\FPTSHOP\\e2_studio\\workspace\\mame_CM33\\src" -I"." -I"C:\\Users\\FPTSHOP\\e2_studio\\workspace\\mame_CM33\\ra\\fsp\\inc" -I"C:\\Users\\FPTSHOP\\e2_studio\\workspace\\mame_CM33\\ra\\fsp\\inc\\api" -I"C:\\Users\\FPTSHOP\\e2_studio\\workspace\\mame_CM33\\ra\\fsp\\inc\\instances" -I"C:\\Users\\FPTSHOP\\e2_studio\\workspace\\mame_CM33\\ra\\arm\\CMSIS_6\\CMSIS\\Core\\Include" -I"C:\\Users\\FPTSHOP\\e2_studio\\workspace\\mame_CM33\\ra_gen" -I"C:\\Users\\FPTSHOP\\e2_studio\\workspace\\mame_CM33\\ra_cfg\\fsp_cfg\\bsp" -I"C:\\Users\\FPTSHOP\\e2_studio\\workspace\\mame_CM33\\ra_cfg\\fsp_cfg" -D_RENESAS_RA_ -D_RA_CORE=CM33 -D_RA_ORDINAL=1 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" -x c "$<" -c -o "$@")
	@clang --target=arm-none-eabi @"$@.in"

