set(RPI4_MEMORY 4096 CACHE STRING "Memory size in MB")

set(RELEASE OFF CACHE BOOL "Performance optimized build")
set(KernelMaxNumNodes 1 CACHE STRING "The cpu core number to be used" FORCE)

set(VmOnDemandDeviceInstall ON CACHE BOOL "Allow the VMM to install arbitrary devices into the VM as they are accessed")
set(VmPCISupport ON CACHE BOOL "Enable virtual pci device support")

# do not override these settings
set(KernelNumDomains 1 CACHE STRING "" FORCE)
set(KernelRootCNodeSizeBits 18 CACHE STRING "" FORCE)
set(KernelArmHypervisorSupport ON CACHE BOOL "" FORCE)
set(KernelArmVtimerUpdateVOffset OFF CACHE BOOL "" FORCE)
set(KernelArmDisableWFIWFETraps ON CACHE BOOL "" FORCE)
set(CapDLLoaderMaxObjects 90000 CACHE STRING "" FORCE)

set(LibUSB OFF CACHE BOOL "" FORCE)

