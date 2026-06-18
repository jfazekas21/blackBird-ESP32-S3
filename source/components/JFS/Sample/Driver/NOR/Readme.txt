Readme.txt for NOR driver

The following hardware / physical layer template files are available:

----------------------------------------------------------------------------------------------------------------
File set                                  Description
----------------------------------------------------------------------------------------------------------------
FS_ConfigNOR_SPI_Template.c               This set of template files can be used for implementing a NOR HW layer
FS_NOR_HW_SPI_Template.c                  for a NOR flash device that is connected via SPI. This configuration
FS_NOR_HW_SPI_Template.h                  uses the Sector Map NOR driver.
----------------------------------------------------------------------------------------------------------------
FS_ConfigNOR_BM_SFDP_Template.c           This set of template files can be used for implementing a NOR HW layer
FS_NOR_HW_SPI_DualQuad_Template.c         for a NOR flash device that is connected via two or four data lines.
FS_NOR_HW_SPI_DualQuad_Template.h         This configuration uses the Block Map NOR driver.
----------------------------------------------------------------------------------------------------------------
FS_ConfigNOR_BM_SPIFI_Template.c          This set of template files can be used for implementing a NOR HW layer
FS_NOR_HW_SPIFI_Template.c                for a NOR flash device that is connected via two or four data lines
FS_NOR_HW_SPIFI_Template.h                and is mapped in the system memory. This configuration uses 
                                          the Block Map NOR driver.     
----------------------------------------------------------------------------------------------------------------
FS_ConfigNOR_BM_SPIFI_Octal_Template.c    This set of template files can be used for implementing a NOR HW layer
FS_NOR_HW_SPIFI_Octal_Template.c          for a NOR flash device that is connected via eight data lines and is
FS_NOR_HW_SPIFI_Octal_Template.h          mapped in the system memory. This configuration uses 
                                          the Block Map NOR driver.
----------------------------------------------------------------------------------------------------------------
FS_ConfigNOR_PHY_Template.c               This set of template files can be used for implementing a NOR physical
FS_NOR_PHY_Template.c                     layer. This configuration uses the Sector Map NOR driver.
FS_NOR_PHY_Template.h
----------------------------------------------------------------------------------------------------------------


