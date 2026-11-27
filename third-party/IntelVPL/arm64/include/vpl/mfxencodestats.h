/*############################################################################
  # Copyright Intel Corporation
  #
  # SPDX-License-Identifier: MIT
  ############################################################################*/

#ifdef ONEVPL_EXPERIMENTAL


#ifndef __MFXENCODESTATS_H__
#define __MFXENCODESTATS_H__
#include "mfxcommon.h"
#include "mfxstructures.h"

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

/*!< The enum to specify memory layout for statistics. */
typedef enum {
     MFX_ENCODESTATS_MEMORY_LAYOUT_DEFAULT                 = 0, /*!< The default memory layout for statistics. */
} mfxEncodeBlkStatsMemLayout;

/*!< The enum to specify mode to gather statistics. */
typedef enum {
     MFX_ENCODESTATS_MODE_DEFAULT                          = 0, /*!< Encode mode is selected by the implementation. */
     MFX_ENCODESTATS_MODE_ENCODE                           = 1, /*!< Full encode mode. */
} mfxEncodeStatsMode;

/*!< Flags to specify what statistics will be reported by the implementation. */
enum {
     MFX_ENCODESTATS_LEVEL_BLK                             = 0x1, /*!< Block level statistics. */
     MFX_ENCODESTATS_LEVEL_SLICE                           = 0x2, /*!< Slice level statistics. */
     MFX_ENCODESTATS_LEVEL_TILE                            = 0x4, /*!< Tile  level statistics. */
     MFX_ENCODESTATS_LEVEL_FRAME                           = 0x8, /*!< Frame level statistics. */
};


MFX_PACK_BEGIN_USUAL_STRUCT()
/*!< Specifies H.265 CTU header. */
typedef struct {
    union {
        struct {
            mfxU32  CUcountminus1         : 6; /*!< Number of CU per CTU. */
            mfxU32  MaxDepth              : 2; /*!< Max quad-tree depth of CU in CTU. */
            mfxU32  reserved              : 24;
        } bitfields0;
        mfxU32      dword0;
    };
    mfxU16  CurrXAddr;               /*!< Horizontal address of CTU. */
    mfxU16  CurrYAddr;               /*!< Vertical address of CTU. */
    mfxU32  reserved1;
} mfxCTUHeader;
MFX_PACK_END()


MFX_PACK_BEGIN_USUAL_STRUCT()
/*!< Specifies H.265 CU info. */
typedef struct {
    union {
        struct {
            mfxU32 CU_Size                 : 2; /*!< indicates the CU size of the current CU. 0: 8x8 1: 16x16 2: 32x32 3: 64x64 */
            mfxU32 CU_pred_mode            : 1; /*!< indicates the prediction mode for the current CU. 0: intra 1: inter */
    /*!
    indicates the PU partition mode for the current CU.
    0: 2Nx2N
    1: 2NxN (inter)
    2: Nx2N (inter)
    3: NXN (intra only, CU Size=8x8 only. Luma Intra Mode indicates the intra prediction mode for 4x4_0. The additional prediction modes are overloaded on 4x4_1, 4x4_2, 4x4_3 below)
    4: 2NxnT (inter only)
    5: 2NxnB (inter only)
    6: nLx2N (inter only)
    7: nRx2N (inter only).
    */
            mfxU32 CU_part_mode         : 3;
            mfxU32 InterPred_IDC_MV0    : 2; /*!< indicates the prediction direction for PU0 of the current CU. 0: L0 1: L1 2: Bi 3: reserved */
            mfxU32 InterPred_IDC_MV1    : 2; /*!< indicates the prediction direction for PU1 of the current CU. 0: L0 1: L1 2: Bi 3: reserved */
    /*!
    Final explicit Luma Intra Mode 4x4_0 for NxN.
    Valid values 0..34
    Note: CU_part_mode==NxN.
    */
            mfxU32 LumaIntraMode         : 6;
    /*!
    indicates the final explicit Luma Intra Mode for the CU.
    0: DM (use Luma mode, from block 0 if NxN)
    1: reserved
    2: Planar
    3: Vertical
    4: Horizontal
    5: DC */
            mfxU32 ChromaIntraMode       : 3;
            mfxU32 reserved              : 13;
        } bitfields0;
        mfxU32     dword0;
    };

    union {
        struct {
    /*!
    Final explicit Luma Intra Mode 4x4_1.
    Valid values 0..34
    Note: CU_part_mode==NxN.
    */
            mfxU32 LumaIntraMode4x4_1  : 6;
    /*!
    Final explicit Luma Intra Mode 4x4_2.
    Valid values 0..34
    Note: CU_part_mode==NxN.
    */
            mfxU32 LumaIntraMode4x4_2  : 6;
    /*!
    Final explicit Luma Intra Mode 4x4_3.
    Valid values 0..34
    Note: CU_part_mode==NxN.
    */
            mfxU32 LumaIntraMode4x4_3  : 6;
            mfxU32 reserved1           : 14;
        } bitfields1;
        mfxU32     dword1;
    };

    mfxI8    QP;            // signed QP value
    mfxU8    reserved2[3];
    /*! distortion measure, approximation to SAD.
        Will deviate significantly (pre, post reconstruction) and due to variation in algorithm.
    */
    mfxU32    SAD;

    /*!
    These parameters indicate motion vectors that are associated with the PU0/PU1 winners
    range [-2048.00..2047.75].
    L0/PU0 - MV[0][0]
    L0/PU1 - MV[0][1]
    L1/PU0 - MV[1][0]
    L1/PU1 - MV[1][1]
    */
    mfxI16Pair MV[2][2];

    union {
        struct {
    /*!
    This parameter indicates the reference index associated with the MV X/Y
    that is populated in the L0_MV0.X and L0_MV0.Y fields. */
            mfxU32 L0_MV0_RefID    : 4;
    /*!
    This parameter indicates the reference index associated with the MV X/Y
    that is populated in the L0_MV1.X and L0_MV1.Y fields. */
            mfxU32 L0_MV1_RefID    : 4;
    /*!
    This parameter indicates the reference index associated with the MV X/Y
    that is populated in the L1_MV0.X and L1_MV0.Y fields. */
            mfxU32 L1_MV0_RefID    : 4;
    /*!
    This parameter indicates the reference index associated with the MV X/Y
    that is populated in the L1_MV1.X and L1_MV1.Y fields. */
            mfxU32 L1_MV1_RefID    : 4;

            mfxU32 reserved3       : 16;
        } bitfields8;
        mfxU32 dword8;
    };
    mfxU32    reserved4[10];
} mfxCUInfo;
MFX_PACK_END()


MFX_PACK_BEGIN_USUAL_STRUCT()
/*!< Specifies H.265 CTU. */
typedef struct {
    mfxCTUHeader    CtuHeader;  /*!< H.265 CTU header. */
    mfxCUInfo       CuInfo[64]; /*!< Array of CU. */
    mfxU32          reserved;
} mfxCTUInfo;
MFX_PACK_END()



MFX_PACK_BEGIN_USUAL_STRUCT()
/*!
   The structure describes H.264 stats per MB.
*/
typedef struct {
    union {
        struct {
      /*!
         Together with @p IntraMbFlag this parameter specifies macroblock type according to the
         ISO\*\/IEC\* 14496-10 with the following difference - it stores either intra or inter
         values according to @p IntraMbFlag, but not intra after inter.
         Values for P-slices are mapped to B-slice values. For example P_16x8 is coded with
         B_FWD_16x8 value.
      */
            mfxU32 MBType         : 5;
      /*!
         This field specifies inter macroblock mode and is ignored for intra MB. It is derived from @p MbType and has next values:
         @li 0 - 16x16 mode
         @li 1 - 16x8 mode
         @li 2 - 8x16 mode
         @li 3 - 8x8  mode
      */
            mfxU32 InterMBMode    : 2;
      /*!
         This field specifies intra macroblock mode and is ignored for inter MB. It is derived from @p MbType and has next values:
         @li 0 - 16x16 mode
         @li 1 - 8x8 mode
         @li 2 - 4x4 mode
         @li 3 - PCM
      */
            mfxU32 IntraMBMode    : 2;
      /*!
         This flag specifies intra/inter MB type and has next values:
         0 - Inter prediction MB type
         1 - Intra prediction MB type
      */
            mfxU32 IntraMBFlag    : 1;
      /*!
         This field specifies subblock shapes for the current MB. Each block is described by 2 bits starting from lower bits for block 0.

         @li 0 - 8x8
         @li 1 - 8x4
         @li 2 - 4x8
         @li 3 - 4x4
      */
            mfxU32 SubMBShapes    : 8;
      /*!
         This field specifies prediction modes for the current MB partition blocks. Each block is described by 2 bits starting from lower bits for block 0.
         @li 0 - Pred_L0
         @li 1 - Pred_L1
         @li 2 - BiPred
         @li 3 - reserved

         Only one prediction value for partition is reported, the rest values are set to zero. For example:
         @li 16x16 Pred_L1 - 0x01 (only 2 lower bits are used)
         @li 16x8 Pred_L1 / BiPred - 0x09 (1001b)
         @li 8x16 BiPred / BiPred - 0x0a (1010b)

         For P MBs this value is always zero.
      */
            mfxU32 SubMBShapeMode  : 8;
      /*!
          This value specifies chroma intra prediction mode.
          @li 0 - DC
          @li 1 - Horizontal
          @li 2 - Vertical
          @li 3 - Plane
      */
            mfxU32 ChromaIntraPredMode  : 2;
            mfxU32 reserved             : 4;
        } bitfields0;
        mfxU32     dword0;
    } ;
      /*!
          Distortion measure, approximation to SAD.
          Deviate significantly (pre, post reconstruction) and due to variation in algorithm.
      */
      mfxU32 SAD;
      mfxI8  Qp; /*!< MB QP. */
      mfxU8  reserved1[3];

      /*!
          These values specify luma intra prediction modes for current MB. Each element of the array
          corresponds to 8x8 block and each holds prediction modes for four 4x4 subblocks.
          Four bits per mode, lowest bits for left top subblock.
          All 16 prediction modes are always specified. For 8x8 case, block prediction mode is
          populated to all subblocks of the 8x8 block. For 16x16 case - to all subblocks of the MB.

          Prediction directions for 4x4 and 8x8 blocks:
          @li 0 - Vertical
          @li 1 - Horizontal
          @li 2 - DC
          @li 3 - Diagonal Down Left
          @li 4 - Diagonal Down Right
          @li 5 - Vertical Right
          @li 6 - Horizontal Down
          @li 7 - Vertical Left
          @li 8 - Horizontal Up

          Prediction directions for 16x16 blocks:
          @li 0 - Vertical
          @li 1 - Horizontal
          @li 2 - DC
          @li 3 - Plane
      */
      mfxU16 LumaIntraMode[4];

      mfxU32 reserved2;
} mfxMBInfo;
MFX_PACK_END()

/*!
   The enum specifies block size.
*/
typedef enum {
    MFX_BLOCK_4X4   = 0, /*!< 4x4 block size. */
    MFX_BLOCK_16X16 = 1, /*!< 16x16 block size. */
} mfxBlockSize;


MFX_PACK_BEGIN_STRUCT_W_PTR()
/*!
   The structure describes H.264 and H.265 stats per MB or CTUs.
*/
typedef struct {
    union {
        mfxU32  NumMB;  /*!< Number of MBs per frame for H.264. */
        mfxU32  NumCTU; /*!< number of CTUs per frame for H.265. */
    };
    union {
        mfxCTUInfo     *HEVCCTUArray; /*!< Array of CTU statistics. */
        mfxMBInfo      *AVCMBArray;   /*!< Array of MB statistics. */
    };
    mfxU32             reserved[8];

} mfxEncodeBlkStats;
MFX_PACK_END()


MFX_PACK_BEGIN_STRUCT_W_L_TYPE()
/*!
   The structure describes H.264/H.265 frame/slice/tile level statistics.
*/
typedef struct {
    mfxF32  PSNRLuma; /*!< PSNR for LUMA samples. */
    mfxF32  PSNRCb;   /*!< PSNR for Chroma (Cb) samples. */
    mfxF32  PSNRCr;   /*!< PSNR for Chroma (Cr) samples. */
    /*! distortion measure, approximation to SAD.
        Will deviate significantly (pre, post reconstruction) and due to variation in algorithm.
    */
    mfxU64  SADLuma;
    mfxF32  Qp;  /*!< average frame QP, may have fractional part in case of MBQP. */

    union {
        mfxU32  NumMB;  /*!< Number of MBs per frame for H.264. */
        mfxU32  NumCTU; /*!< number of CTUs per frame for H.265. */
    };

    mfxBlockSize   BlockSize; /*! For H.264 it is always 16x16 corresponding to MB size.
                               In H.265 it's normalized to 4x4, so for each CU we calculate number of 4x4 which belongs to the block. */

    mfxU32  NumIntraBlock;   /*! Number of intra blocks in the frame. The size of block is defined by BlockSize.
                                 For H.265 it can be more than number of intra CU. */
    mfxU32  NumInterBlock;   /*! Number of inter blocks in the frame. The size of block is defined by BlockSize.
                                 For H.265 it can be more than number of inter CU. */
    mfxU32  NumSkippedBlock; /*! Number of skipped blocks in the frame. The size of block is defined by BlockSize.
                                 For H.265 it can be more than number of skipped CU. */

    mfxU32             reserved[8];

} mfxEncodeHighLevelStats;
MFX_PACK_END()

/*!
   Alias for the structure to describe H.264 and H.265 frame level stats.
*/
typedef mfxEncodeHighLevelStats mfxEncodeFrameStats;

MFX_PACK_BEGIN_STRUCT_W_PTR()
/*!
   The structure describes H.264 and H.265 stats per Slice or Tile.
*/
typedef struct {
    mfxU32                       NumElements;         /*!< Number of Slices or Tiles per frame for H.264/H.265. */
    mfxEncodeHighLevelStats     *HighLevelStatsArray; /*!< Array of CTU statistics. */
    mfxU32                       reserved[8];

} mfxEncodeSliceStats;
MFX_PACK_END()

/*!
   Alias for the structure to describe H.264 and H.265 tile level stats.
*/
typedef mfxEncodeSliceStats mfxEncodeTileStats;


#define MFX_ENCODESTATSCONTAINER_VERSION MFX_STRUCT_VERSION(1, 0)


MFX_PACK_BEGIN_STRUCT_W_PTR()
/*! The structure represents reference counted container for output after encoding operation which includes statistics
    and synchronization primitive for compressed bitstream.
    The memory is allocated and released by the library.
*/
typedef struct mfxEncodeStatsContainer {
    mfxStructVersion    Version; /*!< The version of the structure. */
    mfxRefInterface     RefInterface; /*! < Reference counting interface. */
    /*! @brief
    Guarantees readiness of the statistics after a function completes.
    Instead of MFXVideoCORE_SyncOperation which leads to the synchronization of all output objects,
    users may directly call the mfxEncodeStatsContainer::SynchronizeStatistics function to get output statistics.


    @param[in]   ref_interface  Valid interface.
    @param[out]  wait  Wait time in milliseconds.


    @return
     MFX_ERR_NONE               If no error. \n
     MFX_ERR_NULL_PTR           If interface is NULL. \n
     MFX_ERR_INVALID_HANDLE     If any of container is not valid object . \n
     MFX_WRN_IN_EXECUTION       If the given timeout is expired and the container is not ready. \n
     MFX_ERR_ABORTED            If the specified asynchronous function aborted due to data dependency on a previous asynchronous function that did not complete. \n
     MFX_ERR_UNKNOWN            Any internal error.
    */
    mfxStatus (MFX_CDECL *SynchronizeStatistics)(mfxRefInterface*  ref_interface, mfxU32 wait);
    /*! @brief
    Guarantees readiness of associated compressed bitstream after a function completes.
    Instead of MFXVideoCORE_SyncOperation which leads to the synchronization of all output objects,
    users may directly call the mfxEncodeStatsContainer::SynchronizeStatistics function to get output bitstream.


    @param[in]   ref_interface  Valid interface.
    @param[out]  wait  Wait time in milliseconds.


    @return
     MFX_ERR_NONE               If no error. \n
     MFX_ERR_NULL_PTR           If interface is NULL. \n
     MFX_ERR_INVALID_HANDLE     If any of container is not valid object . \n
     MFX_WRN_IN_EXECUTION       If the given timeout is expired and the container is not ready. \n
     MFX_ERR_ABORTED            If the specified asynchronous function aborted due to data dependency on a previous asynchronous function that did not complete. \n
     MFX_ERR_UNKNOWN            Any internal error.
    */
    mfxStatus (MFX_CDECL *SynchronizeBitstream)(mfxRefInterface*  ref_interface, mfxU32 wait);
    mfxHDL    reserved[4];
    mfxU32    reserved1[2];
    mfxU32                       DisplayOrder;       /*!< To which frame number statistics belong. */
    mfxEncodeBlkStatsMemLayout   MemLayout;          /*!< Memory layout for statistics. */
    mfxEncodeBlkStats           *EncodeBlkStats;     /*!< Block level  statistics. */
    mfxEncodeSliceStats         *EncodeSliceStats;   /*!< Slice level  statistics. */
    mfxEncodeTileStats          *EncodeTileStats;    /*!< Tile  level  statistics. */
    mfxEncodeFrameStats         *EncodeFrameStats;   /*!< Frame level  statistics. */
    mfxU32       reserved2[8];
}mfxEncodeStatsContainer;
MFX_PACK_END()


MFX_PACK_BEGIN_STRUCT_W_PTR()
/*! The extension buffer which should be attached by application for mfxBitstream buffer before
   encode operation. As result the encoder will allocate memory for statistics and fill appropriate structures.
*/
typedef struct {
    mfxExtBuffer Header; /*!< Extension buffer header. Header.BufferId must be equal to MFX_EXTBUFF_ENCODESTATS_BLK. */
    mfxU16                       EncodeStatsFlags;       /*!< What statistics is required: block/slice/tile/frame level or any combinations.
                                                             In case of slice or tile output statistics for one slice or tile will be available only.*/
    mfxEncodeStatsMode           Mode;                   /*!< What encoding mode should be used to gather statistics. */
    mfxEncodeStatsContainer     *EncodeStatsContainer; /*!< encode output, filled by the implementation. */
    mfxU32       reserved[8];
} mfxExtEncodeStatsOutput;
MFX_PACK_END()

#ifdef __cplusplus
} // extern "C"
#endif

#endif

#endif