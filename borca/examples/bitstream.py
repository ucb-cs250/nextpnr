from write_fasm import *
from simple_config import K

# Need to tell FASM generator how to write parameters
# (celltype, parameter) -> ParameterConfig
param_map = {
	("BORCA_SLICE", "K"): ParameterConfig(write=False),
	("BORCA_SLICE", "INIT"): ParameterConfig(write=True, numeric=True, width=2**K),
	("BORCA_SLICE", "FF_USED"): ParameterConfig(write=True, numeric=True, width=1),

	("BORCA_IOB", "INPUT_USED"): ParameterConfig(write=True, numeric=True, width=1),
	("BORCA_IOB", "OUTPUT_USED"): ParameterConfig(write=True, numeric=True, width=1),
	("BORCA_IOB", "ENABLE_USED"): ParameterConfig(write=True, numeric=True, width=1),
}

with open("blinky.fasm", "w") as f:
	write_fasm(ctx, param_map, f)
