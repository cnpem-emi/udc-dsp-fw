################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Each subdirectory must supply rules for building sources it contributes
F28M36x_ELP_DRS/IPC_modules/IPC_modules.obj: C:/Users/ricieri.ohashi/git/C28/F28M36x_ELP_DRS/IPC_modules/IPC_modules.c $(GEN_OPTS) $(GEN_HDRS)
	@echo 'Building file: $<'
	@echo 'Invoking: C2000 Compiler'
	"C:/ti/ccsv6/tools/compiler/c2000_6.2.9/bin/cl2000" -v28 -ml -mt --float_support=fpu32 --vcu_support=vcu0 -O2 --opt_for_speed=1 --include_path="C:/Users/ricieri.ohashi/git/C28/F28M36x_ELP_DRS" --include_path="C:/Users/ricieri.ohashi/git/C28/F28M36x_ELP_DRS/DP_framework" --include_path="C:/Users/ricieri.ohashi/git/C28/F28M36x_ELP_DRS/DP_framework/ELP_DCL" --include_path="C:/Users/ricieri.ohashi/git/C28/F28M36x_ELP_DRS/DP_framework/SigGen" --include_path="C:/Users/ricieri.ohashi/git/C28/F28M36x_ELP_DRS/DP_framework/TimeSlicer" --include_path="C:/Users/ricieri.ohashi/git/C28/F28M36x_ELP_DRS/DP_framework/TI_DCL" --include_path="C:/Users/ricieri.ohashi/git/C28/F28M36x_ELP_DRS/HRADC_board" --include_path="C:/Users/ricieri.ohashi/git/C28/F28M36x_ELP_DRS/IPC_modules" --include_path="C:/Users/ricieri.ohashi/git/C28/F28M36x_ELP_DRS/PS_modules" --include_path="C:/Users/ricieri.ohashi/git/C28/F28M36x_ELP_DRS/PWM_modules" --include_path="C:/ti/ccsv6/tools/compiler/c2000_6.2.9/include" --include_path="C:/Users/ricieri.ohashi/git/C28/F28M36x_common/include" --include_path="C:/Users/ricieri.ohashi/git/C28/F28M36x_headers/include" -g --diag_warning=225 --display_error_number --issue_remarks --diag_wrap=off --preproc_with_compile --preproc_dependency="F28M36x_ELP_DRS/IPC_modules/IPC_modules.pp" --obj_directory="F28M36x_ELP_DRS/IPC_modules" $(GEN_OPTS__FLAG) "$<"
	@echo 'Finished building: $<'
	@echo ' '


