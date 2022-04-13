/*++

Copyright (c) 2000  Microsoft Corporation

Module Name:

    nuiouser.h

Abstract:

    Constants and types to access the NDISPROT driver.
    Users must also include ntddndis.h

Environment:

    User/Kernel mode.

Revision History:

--*/

#ifndef __NPROTUSER__H
#define __NPROTUSER__H


#define FSCTL_NDISPROT_BASE      FILE_DEVICE_NETWORK

#define _NDISPROT_CTL_CODE(_Function, _Method, _Access)  \
            CTL_CODE(FSCTL_NDISPROT_BASE, _Function, _Method, _Access)

#define IOCTL_NDISPROT_OPEN_DEVICE   \
            _NDISPROT_CTL_CODE(0x200, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)

#define IOCTL_NDISPROT_QUERY_OID_VALUE   \
            _NDISPROT_CTL_CODE(0x201, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)

#define IOCTL_NDISPROT_SET_OID_VALUE   \
            _NDISPROT_CTL_CODE(0x205, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)

#define IOCTL_NDISPROT_QUERY_BINDING   \
            _NDISPROT_CTL_CODE(0x203, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)

#define IOCTL_NDISPROT_BIND_WAIT   \
            _NDISPROT_CTL_CODE(0x204, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)


//WB:自定义驱动控制码
//WB:获取网卡列表
#define IOCTL_NDISPROT_GET_DEVICE_LIST   \
            _NDISPROT_CTL_CODE(0x207, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
//WB:设置过滤IP段
#define IOCTL_NDISPROT_SET_HOST_SECCION   \
            _NDISPROT_CTL_CODE(0x208, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)

//WB:获取网卡基本信息(MAC,FrameSize.DescStr)
#define IOCTL_NDISPROT_GET_NETWORK_BASE_INFO   \
            _NDISPROT_CTL_CODE(0x209, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)

//WB:获取网卡传输信息(发包数，收包数，发送字节数，接收字节数)
#define IOCTL_NDISPROT_GET_NETWORK_TRANS_INFO   \
            _NDISPROT_CTL_CODE(0x20A, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)

//WB:关闭网卡
#define IOCTL_NDISPROT_CLOSE_DEVICE   \
            _NDISPROT_CTL_CODE(0x20B, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
//
//  Structure to go with IOCTL_NDISPROT_QUERY_OID_VALUE.
//  The Data part is of variable length, determined by
//  the input buffer length passed to DeviceIoControl.
//
typedef struct _NDISPROT_QUERY_OID
{
    NDIS_OID            Oid;
    NDIS_PORT_NUMBER    PortNumber;
    UCHAR               Data[sizeof(ULONG)];
} NDISPROT_QUERY_OID, *PNDISPROT_QUERY_OID;

//
//  Structure to go with IOCTL_NDISPROT_SET_OID_VALUE.
//  The Data part is of variable length, determined
//  by the input buffer length passed to DeviceIoControl.
//
typedef struct _NDISPROT_SET_OID
{
    NDIS_OID            Oid;
    NDIS_PORT_NUMBER    PortNumber;
    UCHAR               Data[sizeof(ULONG)];
} NDISPROT_SET_OID, *PNDISPROT_SET_OID;


//
//  Structure to go with IOCTL_NDISPROT_QUERY_BINDING.
//  The input parameter is BindingIndex, which is the
//  index into the list of bindings active at the driver.
//  On successful completion, we get back a device name
//  and a device descriptor (friendly name).
//
typedef struct _NDISPROT_QUERY_BINDING
{
    ULONG            BindingIndex;        // 0-based binding number
    ULONG            DeviceNameOffset;    // from start of this struct
    ULONG            DeviceNameLength;    // in bytes
    ULONG            DeviceDescrOffset;    // from start of this struct
    ULONG            DeviceDescrLength;    // in bytes

} NDISPROT_QUERY_BINDING, *PNDISPROT_QUERY_BINDING;
 
#endif // __NPROTUSER__H

