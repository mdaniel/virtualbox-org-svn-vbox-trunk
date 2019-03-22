/** @file
  The driver binding and service binding protocol for IP4 driver.

Copyright (c) 2005 - 2014, Intel Corporation. All rights reserved.<BR>
This program and the accompanying materials
are licensed and made available under the terms and conditions of the BSD License
which accompanies this distribution.  The full text of the license may be found at
http://opensource.org/licenses/bsd-license.php

THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include "Ip4Impl.h"

EFI_DRIVER_BINDING_PROTOCOL gIp4DriverBinding = {
  Ip4DriverBindingSupported,
  Ip4DriverBindingStart,
  Ip4DriverBindingStop,
  0xa,
  NULL,
  NULL
};

/**
  This is the declaration of an EFI image entry point. This entry point is
  the same for UEFI Applications, UEFI OS Loaders, and UEFI Drivers including
  both device drivers and bus drivers.

  The entry point for IP4 driver which install the driver
  binding and component name protocol on its image.

  @param[in]  ImageHandle           The firmware allocated handle for the UEFI image.
  @param[in]  SystemTable           A pointer to the EFI System Table.

  @retval EFI_SUCCESS           The operation completed successfully.
  @retval EFI_OUT_OF_RESOURCES  The request could not be completed due to a lack of resources.

**/
EFI_STATUS
EFIAPI
Ip4DriverEntryPoint (
  IN EFI_HANDLE             ImageHandle,
  IN EFI_SYSTEM_TABLE       *SystemTable
  )
{
  return EfiLibInstallDriverBindingComponentName2 (
           ImageHandle,
           SystemTable,
           &gIp4DriverBinding,
           ImageHandle,
           &gIp4ComponentName,
           &gIp4ComponentName2
           );
}

/**
  Test to see if this driver supports ControllerHandle. This service
  is called by the EFI boot service ConnectController(). In
  order to make drivers as small as possible, there are a few calling
  restrictions for this service. ConnectController() must
  follow these calling restrictions. If any other agent wishes to call
  Supported() it must also follow these calling restrictions.

  @param[in]  This                Protocol instance pointer.
  @param[in]  ControllerHandle    Handle of device to test
  @param[in]  RemainingDevicePath Optional parameter use to pick a specific child
                                  device to start.

  @retval EFI_SUCCESS         This driver supports this device
  @retval EFI_ALREADY_STARTED This driver is already running on this device
  @retval other               This driver does not support this device

**/
EFI_STATUS
EFIAPI
Ip4DriverBindingSupported (
  IN EFI_DRIVER_BINDING_PROTOCOL  * This,
  IN EFI_HANDLE                   ControllerHandle,
  IN EFI_DEVICE_PATH_PROTOCOL     * RemainingDevicePath OPTIONAL
  )
{
  EFI_STATUS                Status;

  //
  // Test for the MNP service binding Protocol
  //
  Status = gBS->OpenProtocol (
                  ControllerHandle,
                  &gEfiManagedNetworkServiceBindingProtocolGuid,
                  NULL,
                  This->DriverBindingHandle,
                  ControllerHandle,
                  EFI_OPEN_PROTOCOL_TEST_PROTOCOL
                  );

  if (EFI_ERROR (Status)) {
    return Status;
  }

  //
  // Test for the Arp service binding Protocol
  //
  Status = gBS->OpenProtocol (
                  ControllerHandle,
                  &gEfiArpServiceBindingProtocolGuid,
                  NULL,
                  This->DriverBindingHandle,
                  ControllerHandle,
                  EFI_OPEN_PROTOCOL_TEST_PROTOCOL
                  );

  return Status;
}

/**
  Clean up a IP4 service binding instance. It will release all
  the resource allocated by the instance. The instance may be
  partly initialized, or partly destroyed. If a resource is
  destroyed, it is marked as that in case the destroy failed and
  being called again later.

  @param[in]  IpSb               The IP4 serviceing binding instance to clean up

  @retval EFI_SUCCESS            The resource used by the instance are cleaned up
  @retval other                  Failed to clean up some of the resources.

**/
EFI_STATUS
Ip4CleanService (
  IN IP4_SERVICE            *IpSb
  );


/**
  Create a new IP4 driver service binding private instance.

  @param  Controller         The controller that has MNP service binding
                             installed
  @param  ImageHandle        The IP4 driver's image handle
  @param  Service            The variable to receive the newly created IP4
                             service.

  @retval EFI_OUT_OF_RESOURCES   Failed to allocate some resource
  @retval EFI_SUCCESS            A new IP4 service binding private is created.
  @retval other                  Other error occurs.

**/
EFI_STATUS
Ip4CreateService (
  IN  EFI_HANDLE            Controller,
  IN  EFI_HANDLE            ImageHandle,
  OUT IP4_SERVICE           **Service
  )
{
  IP4_SERVICE               *IpSb;
  EFI_STATUS                Status;

  ASSERT (Service != NULL);

  *Service = NULL;

  //
  // allocate a service private data then initialize all the filed to
  // empty resources, so if any thing goes wrong when allocating
  // resources, Ip4CleanService can be called to clean it up.
  //
  IpSb = AllocatePool (sizeof (IP4_SERVICE));

  if (IpSb == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  IpSb->Signature                   = IP4_SERVICE_SIGNATURE;
  IpSb->ServiceBinding.CreateChild  = Ip4ServiceBindingCreateChild;
  IpSb->ServiceBinding.DestroyChild = Ip4ServiceBindingDestroyChild;
  IpSb->State                       = IP4_SERVICE_UNSTARTED;

  IpSb->NumChildren                 = 0;
  InitializeListHead (&IpSb->Children);

  InitializeListHead (&IpSb->Interfaces);
  IpSb->DefaultInterface            = NULL;
  IpSb->DefaultRouteTable           = NULL;

  Ip4InitAssembleTable (&IpSb->Assemble);

  IpSb->IgmpCtrl.Igmpv1QuerySeen    = 0;
  InitializeListHead (&IpSb->IgmpCtrl.Groups);

  IpSb->Image                       = ImageHandle;
  IpSb->Controller                  = Controller;

  IpSb->MnpChildHandle              = NULL;
  IpSb->Mnp                         = NULL;

  IpSb->MnpConfigData.ReceivedQueueTimeoutValue = 0;
  IpSb->MnpConfigData.TransmitQueueTimeoutValue = 0;
  IpSb->MnpConfigData.ProtocolTypeFilter        = IP4_ETHER_PROTO;
  IpSb->MnpConfigData.EnableUnicastReceive      = TRUE;
  IpSb->MnpConfigData.EnableMulticastReceive    = TRUE;
  IpSb->MnpConfigData.EnableBroadcastReceive    = TRUE;
  IpSb->MnpConfigData.EnablePromiscuousReceive  = FALSE;
  IpSb->MnpConfigData.FlushQueuesOnReset        = TRUE;
  IpSb->MnpConfigData.EnableReceiveTimestamps   = FALSE;
  IpSb->MnpConfigData.DisableBackgroundPolling  = FALSE;

  ZeroMem (&IpSb->SnpMode, sizeof (EFI_SIMPLE_NETWORK_MODE));

  IpSb->Timer                       = NULL;
  IpSb->Ip4Config                   = NULL;
  IpSb->DoneEvent                   = NULL;
  IpSb->ReconfigEvent               = NULL;
  IpSb->ActiveEvent                 = NULL;

  //
  // Create various resources. First create the route table, timer
  // event and MNP child. IGMP, interface's initialization depend
  // on the MNP child.
  //
  IpSb->DefaultRouteTable = Ip4CreateRouteTable ();

  if (IpSb->DefaultRouteTable == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto ON_ERROR;
  }

  Status = gBS->CreateEvent (
                  EVT_NOTIFY_SIGNAL | EVT_TIMER,
                  TPL_CALLBACK,
                  Ip4TimerTicking,
                  IpSb,
                  &IpSb->Timer
                  );

  if (EFI_ERROR (Status)) {
    goto ON_ERROR;
  }

  Status = NetLibCreateServiceChild (
             Controller,
             ImageHandle,
             &gEfiManagedNetworkServiceBindingProtocolGuid,
             &IpSb->MnpChildHandle
             );

  if (EFI_ERROR (Status)) {
    goto ON_ERROR;
  }

  Status = gBS->OpenProtocol (
                  IpSb->MnpChildHandle,
                  &gEfiManagedNetworkProtocolGuid,
                  (VOID **) &IpSb->Mnp,
                  ImageHandle,
                  Controller,
                  EFI_OPEN_PROTOCOL_BY_DRIVER
                  );

  if (EFI_ERROR (Status)) {
    goto ON_ERROR;
  }

  Status = Ip4ServiceConfigMnp (IpSb, TRUE);

  if (EFI_ERROR (Status)) {
    goto ON_ERROR;
  }

  Status = IpSb->Mnp->GetModeData (IpSb->Mnp, NULL, &IpSb->SnpMode);

  if (EFI_ERROR (Status)) {
    goto ON_ERROR;
  }

  Status = Ip4InitIgmp (IpSb);

  if (EFI_ERROR (Status)) {
    goto ON_ERROR;
  }

  IpSb->DefaultInterface = Ip4CreateInterface (IpSb->Mnp, Controller, ImageHandle);

  if (IpSb->DefaultInterface == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto ON_ERROR;
  }

  InsertHeadList (&IpSb->Interfaces, &IpSb->DefaultInterface->Link);

  IpSb->MaxPacketSize = IpSb->SnpMode.MaxPacketSize - sizeof (IP4_HEAD);
  if (NetLibGetVlanId (IpSb->Controller) != 0) {
    //
    // This is a VLAN device, reduce MTU by VLAN tag length
    //
    IpSb->MaxPacketSize -= NET_VLAN_TAG_LEN;
  }
  IpSb->OldMaxPacketSize = IpSb->MaxPacketSize;
  *Service = IpSb;
  return EFI_SUCCESS;

ON_ERROR:
  Ip4CleanService (IpSb);
  FreePool (IpSb);

  return Status;
}


/**
  Clean up a IP4 service binding instance. It will release all
  the resource allocated by the instance. The instance may be
  partly initialized, or partly destroyed. If a resource is
  destroyed, it is marked as that in case the destroy failed and
  being called again later.

  @param[in]  IpSb               The IP4 serviceing binding instance to clean up

  @retval EFI_SUCCESS            The resource used by the instance are cleaned up
  @retval other                  Failed to clean up some of the resources.

**/
EFI_STATUS
Ip4CleanService (
  IN IP4_SERVICE            *IpSb
  )
{
  EFI_STATUS                Status;

  if (IpSb->DefaultInterface != NULL) {
    Status = Ip4FreeInterface (IpSb->DefaultInterface, NULL);

    if (EFI_ERROR (Status)) {
      return Status;
    }

    IpSb->DefaultInterface = NULL;
  }

  if (IpSb->DefaultRouteTable != NULL) {
    Ip4FreeRouteTable (IpSb->DefaultRouteTable);
    IpSb->DefaultRouteTable = NULL;
  }

  Ip4CleanAssembleTable (&IpSb->Assemble);

  if (IpSb->MnpChildHandle != NULL) {
    if (IpSb->Mnp != NULL) {
      gBS->CloseProtocol (
             IpSb->MnpChildHandle,
             &gEfiManagedNetworkProtocolGuid,
             IpSb->Image,
             IpSb->Controller
             );

      IpSb->Mnp = NULL;
    }

    NetLibDestroyServiceChild (
      IpSb->Controller,
      IpSb->Image,
      &gEfiManagedNetworkServiceBindingProtocolGuid,
      IpSb->MnpChildHandle
      );

    IpSb->MnpChildHandle = NULL;
  }

  if (IpSb->Timer != NULL) {
    gBS->SetTimer (IpSb->Timer, TimerCancel, 0);
    gBS->CloseEvent (IpSb->Timer);

    IpSb->Timer = NULL;
  }

  if (IpSb->Ip4Config != NULL) {
    IpSb->Ip4Config->Stop (IpSb->Ip4Config);

    gBS->CloseProtocol (
          IpSb->Controller,
          &gEfiIp4ConfigProtocolGuid,
          IpSb->Image,
          IpSb->Controller
          );

    gBS->CloseEvent (IpSb->DoneEvent);
    gBS->CloseEvent (IpSb->ReconfigEvent);
    IpSb->ActiveEvent = NULL;
    IpSb->Ip4Config = NULL;
  }

  return EFI_SUCCESS;
}

/**
  Callback function which provided by user to remove one node in NetDestroyLinkList process.

  @param[in]    Entry           The entry to be removed.
  @param[in]    Context         Pointer to the callback context corresponds to the Context in NetDestroyLinkList.

  @retval EFI_SUCCESS           The entry has been removed successfully.
  @retval Others                Fail to remove the entry.

**/
EFI_STATUS
EFIAPI
Ip4DestroyChildEntryInHandleBuffer (
  IN LIST_ENTRY         *Entry,
  IN VOID               *Context
  )
{
  IP4_PROTOCOL                  *IpInstance;
  EFI_SERVICE_BINDING_PROTOCOL  *ServiceBinding;
  UINTN                         NumberOfChildren;
  EFI_HANDLE                    *ChildHandleBuffer;

  if (Entry == NULL || Context == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  IpInstance = NET_LIST_USER_STRUCT_S (Entry, IP4_PROTOCOL, Link, IP4_PROTOCOL_SIGNATURE);
  ServiceBinding    = ((IP4_DESTROY_CHILD_IN_HANDLE_BUF_CONTEXT *) Context)->ServiceBinding;
  NumberOfChildren  = ((IP4_DESTROY_CHILD_IN_HANDLE_BUF_CONTEXT *) Context)->NumberOfChildren;
  ChildHandleBuffer = ((IP4_DESTROY_CHILD_IN_HANDLE_BUF_CONTEXT *) Context)->ChildHandleBuffer;

  if (!NetIsInHandleBuffer (IpInstance->Handle, NumberOfChildren, ChildHandleBuffer)) {
    return EFI_SUCCESS;
  }

  return ServiceBinding->DestroyChild (ServiceBinding, IpInstance->Handle);
}

/**
  Start this driver on ControllerHandle. This service is called by the
  EFI boot service ConnectController(). In order to make
  drivers as small as possible, there are a few calling restrictions for
  this service. ConnectController() must follow these
  calling restrictions. If any other agent wishes to call Start() it
  must also follow these calling restrictions.

  @param[in]  This                 Protocol instance pointer.
  @param[in]  ControllerHandle     Handle of device to bind driver to
  @param[in]  RemainingDevicePath  Optional parameter use to pick a specific child
                                   device to start.

  @retval EFI_SUCCESS          This driver is added to ControllerHandle
  @retval EFI_ALREADY_STARTED  This driver is already running on ControllerHandle
  @retval other                This driver does not support this device

**/
EFI_STATUS
EFIAPI
Ip4DriverBindingStart (
  IN EFI_DRIVER_BINDING_PROTOCOL  * This,
  IN EFI_HANDLE                   ControllerHandle,
  IN EFI_DEVICE_PATH_PROTOCOL     * RemainingDevicePath OPTIONAL
  )
{
  IP4_SERVICE               *IpSb;
  EFI_STATUS                Status;

  //
  // Test for the Ip4 service binding protocol
  //
  Status = gBS->OpenProtocol (
                  ControllerHandle,
                  &gEfiIp4ServiceBindingProtocolGuid,
                  NULL,
                  This->DriverBindingHandle,
                  ControllerHandle,
                  EFI_OPEN_PROTOCOL_TEST_PROTOCOL
                  );

  if (Status == EFI_SUCCESS) {
    return EFI_ALREADY_STARTED;
  }

  Status = Ip4CreateService (ControllerHandle, This->DriverBindingHandle, &IpSb);

  if (EFI_ERROR (Status)) {
    return Status;
  }
  ASSERT (IpSb != NULL);

  //
  // Install the Ip4ServiceBinding Protocol onto ControlerHandle
  //
  Status = gBS->InstallMultipleProtocolInterfaces (
                  &ControllerHandle,
                  &gEfiIp4ServiceBindingProtocolGuid,
                  &IpSb->ServiceBinding,
                  NULL
                  );

  if (EFI_ERROR (Status)) {
    goto FREE_SERVICE;
  }

  //
  // ready to go: start the receiving and timer
  //
  Status = Ip4ReceiveFrame (IpSb->DefaultInterface, NULL, Ip4AccpetFrame, IpSb);

  if (EFI_ERROR (Status)) {
    goto UNINSTALL_PROTOCOL;
  }

  Status = gBS->SetTimer (IpSb->Timer, TimerPeriodic, TICKS_PER_SECOND);

  if (EFI_ERROR (Status)) {
    goto UNINSTALL_PROTOCOL;
  }

  //
  // Initialize the IP4 ID
  //
  mIp4Id = (UINT16)NET_RANDOM (NetRandomInitSeed ());

  return Status;

UNINSTALL_PROTOCOL:
  gBS->UninstallProtocolInterface (
         ControllerHandle,
         &gEfiIp4ServiceBindingProtocolGuid,
         &IpSb->ServiceBinding
         );

FREE_SERVICE:
  Ip4CleanService (IpSb);
  FreePool (IpSb);

  return Status;
}


/**
  Stop this driver on ControllerHandle. This service is called by the
  EFI boot service DisconnectController(). In order to
  make drivers as small as possible, there are a few calling
  restrictions for this service. DisconnectController()
  must follow these calling restrictions. If any other agent wishes
  to call Stop() it must also follow these calling restrictions.

  @param[in]  This              Protocol instance pointer.
  @param[in]  ControllerHandle  Handle of device to stop driver on
  @param[in]  NumberOfChildren  Number of Handles in ChildHandleBuffer. If number
                                of children is zero stop the entire bus driver.
  @param[in]  ChildHandleBuffer List of Child Handles to Stop.

  @retval EFI_SUCCESS           This driver is removed ControllerHandle
  @retval other                 This driver was not removed from this device

**/
EFI_STATUS
EFIAPI
Ip4DriverBindingStop (
  IN  EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN  EFI_HANDLE                   ControllerHandle,
  IN  UINTN                        NumberOfChildren,
  IN  EFI_HANDLE                   *ChildHandleBuffer
  )
{
  EFI_SERVICE_BINDING_PROTOCOL             *ServiceBinding;
  IP4_SERVICE                              *IpSb;
  EFI_HANDLE                               NicHandle;
  EFI_STATUS                               Status;
  INTN                                     State;
  LIST_ENTRY                               *List;
  IP4_DESTROY_CHILD_IN_HANDLE_BUF_CONTEXT  Context;
  IP4_INTERFACE                            *IpIf;
  IP4_ROUTE_TABLE                          *RouteTable;

  //
  // IP4 driver opens the MNP child, ARP children or the IP4_CONFIG protocol
  // by driver. So the ControllerHandle may be the MNP child handle, ARP child
  // handle, or the NIC (UNDI) handle because IP4_CONFIG protocol is installed
  // in the NIC handle.
  //
  //
  // First, check whether it is the IP4_CONFIG protocol being uninstalled.
  // IP4_CONFIG protocol is installed on the NIC handle. It isn't necessary
  // to clean up the default configuration if IP4_CONFIG is being stopped.
  //
  Status = gBS->OpenProtocol (
                  ControllerHandle,
                  &gEfiIp4ConfigProtocolGuid,
                  NULL,
                  This->DriverBindingHandle,
                  ControllerHandle,
                  EFI_OPEN_PROTOCOL_TEST_PROTOCOL
                  );
  if (Status == EFI_SUCCESS) {
    //
    // Retrieve the IP4 service binding protocol. If failed, it is
    // likely that Ip4 ServiceBinding is uninstalled already. In this
    // case, return immediately.
    //
    Status = gBS->OpenProtocol (
                    ControllerHandle,
                    &gEfiIp4ServiceBindingProtocolGuid,
                    (VOID **) &ServiceBinding,
                    This->DriverBindingHandle,
                    ControllerHandle,
                    EFI_OPEN_PROTOCOL_GET_PROTOCOL
                    );
    if (EFI_ERROR (Status)) {
      return EFI_DEVICE_ERROR;
    }

    IpSb = IP4_SERVICE_FROM_PROTOCOL (ServiceBinding);
    if (IpSb->Ip4Config != NULL && (IpSb->State != IP4_SERVICE_DESTROY)) {

      IpSb->Ip4Config->Stop (IpSb->Ip4Config);

      Status = gBS->CloseProtocol (
                      ControllerHandle,
                      &gEfiIp4ConfigProtocolGuid,
                      IpSb->Image,
                      ControllerHandle
                      );
      if (EFI_ERROR (Status)) {
        return Status;
      }

      //
      // If the auto configure hasn't complete, mark it as not started.
      //
      if (IpSb->State == IP4_SERVICE_STARTED) {
        IpSb->State = IP4_SERVICE_UNSTARTED;
      }

      IpSb->Ip4Config = NULL;
      gBS->CloseEvent (IpSb->DoneEvent);
      gBS->CloseEvent (IpSb->ReconfigEvent);
    }

    return EFI_SUCCESS;
  }

  //
  // Either MNP or ARP protocol is being uninstalled. The controller
  // handle is either the MNP child or ARP child. But, the IP4's
  // service binding is installed on the NIC handle. So, need to open
  // the protocol info to find the NIC handle.
  //
  NicHandle = NetLibGetNicHandle (ControllerHandle, &gEfiManagedNetworkProtocolGuid);
  if (NicHandle == NULL) {
    NicHandle = NetLibGetNicHandle (ControllerHandle, &gEfiArpProtocolGuid);
    if (NicHandle == NULL) {
      return EFI_SUCCESS;
    }
  }

  //
  // Retrieve the IP4 service binding protocol
  //
  Status = gBS->OpenProtocol (
                  NicHandle,
                  &gEfiIp4ServiceBindingProtocolGuid,
                  (VOID **) &ServiceBinding,
                  This->DriverBindingHandle,
                  NicHandle,
                  EFI_OPEN_PROTOCOL_GET_PROTOCOL
                  );
  if (EFI_ERROR (Status)) {
    return EFI_DEVICE_ERROR;
  }

  IpSb   = IP4_SERVICE_FROM_PROTOCOL (ServiceBinding);
  if (NumberOfChildren != 0) {
    List = &IpSb->Children;
    Context.ServiceBinding    = ServiceBinding;
    Context.NumberOfChildren  = NumberOfChildren;
    Context.ChildHandleBuffer = ChildHandleBuffer;
    Status = NetDestroyLinkList (
               List,
               Ip4DestroyChildEntryInHandleBuffer,
               &Context,
               NULL
               );
  } else if (IpSb->DefaultInterface->ArpHandle == ControllerHandle) {
    //
    // The ARP protocol for the default interface is being uninstalled and all
    // its IP child handles should have been destroyed before. So, release the
    // default interface and route table, create a new one and mark it as not started.
    //
    Ip4CancelReceive (IpSb->DefaultInterface);
    Ip4FreeInterface (IpSb->DefaultInterface, NULL);
    Ip4FreeRouteTable (IpSb->DefaultRouteTable);

    IpIf = Ip4CreateInterface (IpSb->Mnp, IpSb->Controller, IpSb->Image);
    if (IpIf == NULL) {
      goto ON_ERROR;
    }
    RouteTable = Ip4CreateRouteTable ();
    if (RouteTable == NULL) {
      Ip4FreeInterface (IpIf, NULL);
      goto ON_ERROR;;
    }

    IpSb->DefaultInterface  = IpIf;
    InsertHeadList (&IpSb->Interfaces, &IpIf->Link);
    IpSb->DefaultRouteTable = RouteTable;
    Ip4ReceiveFrame (IpIf, NULL, Ip4AccpetFrame, IpSb);

    if (IpSb->Ip4Config != NULL && IpSb->State != IP4_SERVICE_DESTROY) {
      IpSb->Ip4Config->Stop (IpSb->Ip4Config);
    }
    IpSb->State = IP4_SERVICE_UNSTARTED;
  } else if (IsListEmpty (&IpSb->Children)) {
    State           = IpSb->State;
    IpSb->State     = IP4_SERVICE_DESTROY;

    //
    // OK, clean other resources then uninstall the service binding protocol.
    //
    Status = Ip4CleanService (IpSb);
    if (EFI_ERROR (Status)) {
      IpSb->State = State;
      goto ON_ERROR;
    }

    gBS->UninstallProtocolInterface (
           NicHandle,
           &gEfiIp4ServiceBindingProtocolGuid,
           ServiceBinding
           );

    if (gIp4ControllerNameTable != NULL) {
      FreeUnicodeStringTable (gIp4ControllerNameTable);
      gIp4ControllerNameTable = NULL;
    }
    FreePool (IpSb);
  }

ON_ERROR:
  return Status;
}


/**
  Creates a child handle and installs a protocol.

  The CreateChild() function installs a protocol on ChildHandle.
  If ChildHandle is a pointer to NULL, then a new handle is created and returned in ChildHandle.
  If ChildHandle is not a pointer to NULL, then the protocol installs on the existing ChildHandle.

  @param  This        Pointer to the EFI_SERVICE_BINDING_PROTOCOL instance.
  @param  ChildHandle Pointer to the handle of the child to create. If it is NULL,
                      then a new handle is created. If it is a pointer to an existing UEFI handle,
                      then the protocol is added to the existing UEFI handle.

  @retval EFI_SUCCES            The protocol was added to ChildHandle.
  @retval EFI_INVALID_PARAMETER ChildHandle is NULL.
  @retval EFI_OUT_OF_RESOURCES  There are not enough resources availabe to create
                                the child
  @retval other                 The child handle was not created

**/
EFI_STATUS
EFIAPI
Ip4ServiceBindingCreateChild (
  IN EFI_SERVICE_BINDING_PROTOCOL  *This,
  IN OUT EFI_HANDLE                *ChildHandle
  )
{
  IP4_SERVICE               *IpSb;
  IP4_PROTOCOL              *IpInstance;
  EFI_TPL                   OldTpl;
  EFI_STATUS                Status;
  VOID                      *Mnp;

  if ((This == NULL) || (ChildHandle == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  IpSb       = IP4_SERVICE_FROM_PROTOCOL (This);
  IpInstance = AllocatePool (sizeof (IP4_PROTOCOL));

  if (IpInstance == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  Ip4InitProtocol (IpSb, IpInstance);

  //
  // Install Ip4 onto ChildHandle
  //
  Status = gBS->InstallMultipleProtocolInterfaces (
                  ChildHandle,
                  &gEfiIp4ProtocolGuid,
                  &IpInstance->Ip4Proto,
                  NULL
                  );

  if (EFI_ERROR (Status)) {
    goto ON_ERROR;
  }

  IpInstance->Handle = *ChildHandle;

  //
  // Open the Managed Network protocol BY_CHILD.
  //
  Status = gBS->OpenProtocol (
                  IpSb->MnpChildHandle,
                  &gEfiManagedNetworkProtocolGuid,
                  (VOID **) &Mnp,
                  gIp4DriverBinding.DriverBindingHandle,
                  IpInstance->Handle,
                  EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER
                  );
  if (EFI_ERROR (Status)) {
    gBS->UninstallMultipleProtocolInterfaces (
           ChildHandle,
           &gEfiIp4ProtocolGuid,
           &IpInstance->Ip4Proto,
           NULL
           );

    goto ON_ERROR;
  }

  //
  // Insert it into the service binding instance.
  //
  OldTpl = gBS->RaiseTPL (TPL_CALLBACK);

  InsertTailList (&IpSb->Children, &IpInstance->Link);
  IpSb->NumChildren++;

  gBS->RestoreTPL (OldTpl);

ON_ERROR:

  if (EFI_ERROR (Status)) {

    Ip4CleanProtocol (IpInstance);

    FreePool (IpInstance);
  }

  return Status;
}


/**
  Destroys a child handle with a protocol installed on it.

  The DestroyChild() function does the opposite of CreateChild(). It removes a protocol
  that was installed by CreateChild() from ChildHandle. If the removed protocol is the
  last protocol on ChildHandle, then ChildHandle is destroyed.

  @param  This        Pointer to the EFI_SERVICE_BINDING_PROTOCOL instance.
  @param  ChildHandle Handle of the child to destroy

  @retval EFI_SUCCES            The protocol was removed from ChildHandle.
  @retval EFI_UNSUPPORTED       ChildHandle does not support the protocol that is being removed.
  @retval EFI_INVALID_PARAMETER Child handle is NULL.
  @retval EFI_ACCESS_DENIED     The protocol could not be removed from the ChildHandle
                                because its services are being used.
  @retval other                 The child handle was not destroyed

**/
EFI_STATUS
EFIAPI
Ip4ServiceBindingDestroyChild (
  IN EFI_SERVICE_BINDING_PROTOCOL  *This,
  IN EFI_HANDLE                    ChildHandle
  )
{
  EFI_STATUS                Status;
  IP4_SERVICE               *IpSb;
  IP4_PROTOCOL              *IpInstance;
  EFI_IP4_PROTOCOL          *Ip4;
  EFI_TPL                   OldTpl;
  INTN                      State;

  if ((This == NULL) || (ChildHandle == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  //
  // Retrieve the private context data structures
  //
  IpSb   = IP4_SERVICE_FROM_PROTOCOL (This);

  Status = gBS->OpenProtocol (
                  ChildHandle,
                  &gEfiIp4ProtocolGuid,
                  (VOID **) &Ip4,
                  gIp4DriverBinding.DriverBindingHandle,
                  ChildHandle,
                  EFI_OPEN_PROTOCOL_GET_PROTOCOL
                  );

  if (EFI_ERROR (Status)) {
    return EFI_UNSUPPORTED;
  }

  IpInstance = IP4_INSTANCE_FROM_PROTOCOL (Ip4);

  if (IpInstance->Service != IpSb) {
    return EFI_INVALID_PARAMETER;
  }

  OldTpl = gBS->RaiseTPL (TPL_CALLBACK);

  //
  // A child can be destroyed more than once. For example,
  // Ip4DriverBindingStop will destroy all of its children.
  // when UDP driver is being stopped, it will destroy all
  // the IP child it opens.
  //
  if (IpInstance->State == IP4_STATE_DESTROY) {
    gBS->RestoreTPL (OldTpl);
    return EFI_SUCCESS;
  }

  State             = IpInstance->State;
  IpInstance->State = IP4_STATE_DESTROY;

  //
  // Close the Managed Network protocol.
  //
  gBS->CloseProtocol (
         IpSb->MnpChildHandle,
         &gEfiManagedNetworkProtocolGuid,
         gIp4DriverBinding.DriverBindingHandle,
         ChildHandle
         );

  if (IpInstance->Interface != NULL && IpInstance->Interface->Arp != NULL) {
    gBS->CloseProtocol (
           IpInstance->Interface->ArpHandle,
           &gEfiArpProtocolGuid,
           gIp4DriverBinding.DriverBindingHandle,
           ChildHandle
           );
  }

  //
  // Uninstall the IP4 protocol first. Many thing happens during
  // this:
  // 1. The consumer of the IP4 protocol will be stopped if it
  // opens the protocol BY_DRIVER. For eaxmple, if MNP driver is
  // stopped, IP driver's stop function will be called, and uninstall
  // EFI_IP4_PROTOCOL will trigger the UDP's stop function. This
  // makes it possible to create the network stack bottom up, and
  // stop it top down.
  // 2. the upper layer will recycle the received packet. The recycle
  // event's TPL is higher than this function. The recycle events
  // will be called back before preceeding. If any packets not recycled,
  // that means there is a resource leak.
  //
  gBS->RestoreTPL (OldTpl);
  Status = gBS->UninstallProtocolInterface (
                  ChildHandle,
                  &gEfiIp4ProtocolGuid,
                  &IpInstance->Ip4Proto
                  );
  OldTpl = gBS->RaiseTPL (TPL_CALLBACK);
  if (EFI_ERROR (Status)) {
    goto ON_ERROR;
  }

  Status = Ip4CleanProtocol (IpInstance);
  if (EFI_ERROR (Status)) {
    gBS->InstallMultipleProtocolInterfaces (
           &ChildHandle,
           &gEfiIp4ProtocolGuid,
           Ip4,
           NULL
           );

    goto ON_ERROR;
  }

  RemoveEntryList (&IpInstance->Link);
  IpSb->NumChildren--;

  gBS->RestoreTPL (OldTpl);

  FreePool (IpInstance);
  return EFI_SUCCESS;

ON_ERROR:
  IpInstance->State = State;
  gBS->RestoreTPL (OldTpl);

  return Status;
}
