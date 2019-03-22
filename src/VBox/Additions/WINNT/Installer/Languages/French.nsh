; $Id$
;; @file
; NLS for French language.
;

;
; Copyright (C) 2006-2014 Oracle Corporation
;
; This file is part of VirtualBox Open Source Edition (OSE), as
; available from http://www.virtualbox.org. This file is free software;
; you can redistribute it and/or modify it under the terms of the GNU
; General Public License (GPL) as published by the Free Software
; Foundation, in version 2 as it comes in the "COPYING" file of the
; VirtualBox OSE distribution. VirtualBox OSE is distributed in the
; hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
;

LangString VBOX_TEST ${LANG_FRENCH}                                 "Ceci est un message de test de $(^Name)!"

LangString VBOX_NOADMIN ${LANG_FRENCH}                              "Vous avez besoin de droits d'administrateur pour (d�s)installer $(^Name)!$\r$\nCe programme d'installation se terminera maintenant."

LangString VBOX_NOTICE_ARCH_X86 ${LANG_FRENCH}                      "Cette application peut seulement �tre execut�e sur des syst�mes Windows 32-bit. Veuillez installer la version 64-bit de $(^Name)!"
LangString VBOX_NOTICE_ARCH_AMD64 ${LANG_FRENCH}                    "Cette application peut seulement �tre execut�e sur des syst�mes Windows 64-bit. Veuillez installer la version 32-bit de $(^Name)!"
LangString VBOX_NT4_NO_SP6 ${LANG_FRENCH}                           "Le programme d'installation a d�t�ct� que vous utilisez Windows NT4 sans Service Pack 6.$\r$\nNous vous conseillons d'installer ce Service Pack avant de continuer. D�sirez vous cependant continuer?"

LangString VBOX_PLATFORM_UNSUPPORTED ${LANG_FRENCH}                 "Les Additions invit� ne sont pas encore support�s sur cette plateforme!"

LangString VBOX_SUN_FOUND ${LANG_FRENCH}                            "Une ancienne version des Additions invit� Sun est install�e dans cette machine virtuelle. Les Additions invit� actuelles ne peuvent �tre install�es avant que cette version ne soit d�sinstall�e.$\r$\n$\r$\nVoulez-vous d�sinstaller l'ancienne version maintenant?"
LangString VBOX_SUN_ABORTED ${LANG_FRENCH}                          "Le programme ne peut pas continuer avec l'installation des Additions invit�.$\r$\nVeuillez d�sinstaller d'abord les anciennes Additions Sun!"

LangString VBOX_INNOTEK_FOUND ${LANG_FRENCH}                        "Une ancienne version des Additions invit� est install�e dans cette machine virtuelle. Les Additions invit� actuelles ne peuvent �tre install�es avant que cette version ne soit d�sinstall�e.$\r$\n$\r$\nVoulez-vous d�sinstaller l'ancienne version maintenant?"
LangString VBOX_INNOTEK_ABORTED ${LANG_FRENCH}                      "Le programme ne peut pas continuer avec l'installation des Additions invit�.$\r$\nVeuillez d�sinstaller d'abord les anciennes Additions!"

LangString VBOX_UNINSTALL_START ${LANG_FRENCH}                      "Choisissez OK pour d�marrer la d�sinstallation.$\r$\nLe processus n�cessitera quelque temps et se d�roulera en arri�re-plan."
LangString VBOX_UNINSTALL_REBOOT ${LANG_FRENCH}                     "Nous vous conseillons fortement de red�marer cette machine virtuelle avant d'installer la nouvelle version des Additions invit�.$\r$\nVeuillez recommencer l'installation des Additions apr�s le red�marrage.$\r$\n$\r$\nRed�marrer maintenant?"

LangString VBOX_COMPONENT_MAIN ${LANG_FRENCH}                       "Additions invit� VirtualBox"
LangString VBOX_COMPONENT_MAIN_DESC ${LANG_FRENCH}                  "Fichiers prinipaux des Additions invit� VirtualBox"

LangString VBOX_COMPONENT_AUTOLOGON ${LANG_FRENCH}                  "Support identification automatique"
LangString VBOX_COMPONENT_AUTOLOGON_DESC ${LANG_FRENCH}             "Active l'identification automatique dans l'invit�"
LangString VBOX_COMPONENT_AUTOLOGON_WARN_3RDPARTY ${LANG_FRENCH}    "Un composant permettant l'identification automatique est d�j� install�.$\r$\nSi vous le remplac� avec le composant issue de VirtualBox, cela pourrait d�stabiliser le syst�me.$\r$\nD�sirez-vous cependant continuer?"

LangString VBOX_COMPONENT_D3D  ${LANG_FRENCH}                       "Support Direct3D pour invit�s (experimental)"
LangString VBOX_COMPONENT_D3D_DESC  ${LANG_FRENCH}                  "Active le support Direct3D pour invit�s (experimental)"
LangString VBOX_COMPONENT_D3D_NO_SM ${LANG_FRENCH}                  "Windows ne fonctionne pas en mode sans �chec.$\r$\nDe ce fait, le support D3D ne peut �tre install�."
LangString VBOX_COMPONENT_D3D_NOT_SUPPORTED ${LANG_FRENCH}          "Le support invit� pour Direct3D n'est pas disponible sur Windows $g_strWinVersion!"
LangString VBOX_COMPONENT_D3D_OR_WDDM ${LANG_FRENCH}                "Ce syst�me supporte l'interface Windows Aero (WDDM).$\r$\nLe support VirtualBox pour cette fonctionalit� est exp�rimental et ne devrait pas encore �tre utilis� sur des syst�mes importants.$\r$\n$\r$\nVoulez-vous installer le support Direct3D de base � la place?"
LangString VBOX_COMPONENT_D3D_HINT_VRAM ${LANG_FRENCH}              "Veuillez noter que l'utilisation de l'acc�l�ration 3D n�c�cssite au moins 128 MB de m�moire vid�o ; pour un utilisation avec plusieurs �crans nous recommandons  d'affecter 256 MB.$\r$\n$\r$\nSi n�c�ssaire vous pouvez changer la taille du m�moire vid�o dans la sous-section $\"Affichage$\" des param�tres de la machine virtuelle."
LangString VBOX_COMPONENT_D3D_INVALID ${LANG_FRENCH}                "Le programme d'installation a d�tect� une installation DirectX corrompue ou invalide.$\r$\n$\r$\nAfin d'assurer le bon fonctionnement du support DirectX, nous conseillons de r�installer le moteur d'ex�cution DirectX.$\r$\n$\r$\nD�sirez-vous cependant continuer?"
LangString VBOX_COMPONENT_D3D_INVALID_MANUAL ${LANG_FRENCH}         "Voulez-vous voir le manuel d'utilisateur VirtualBox pour chercher une solution?"

LangString VBOX_COMPONENT_STARTMENU ${LANG_FRENCH}                  "Start menu entries"
LangString VBOX_COMPONENT_STARTMENU_DESC ${LANG_FRENCH}             "Creates entries in the start menu"

LangString VBOX_WFP_WARN_REPLACE ${LANG_FRENCH}                     "Le programme d'installation vient de remplacer certains fichiers syst�mes afin de faire fonctionner correctement ${PRODUCT_NAME}.$\r$\nPour le cas qu'un avertissement de la Protection de fichiers Windows apparaisse, veuiller l'annuler sans restaurer les fichiers originaux!"
LangString VBOX_REBOOT_REQUIRED ${LANG_FRENCH}                      "Le syst�me doit �tre red�marr� pourque les changements prennent effet. Red�marrer Windows maintenant?"

LangString VBOX_EXTRACTION_COMPLETE ${LANG_FRENCH}                  "$(^Name): Les fichiers ont �t� extrait avec succ�s dans $\"$INSTDIR$\"!"

LangString VBOX_ERROR_INST_FAILED ${LANG_FRENCH}                    "Une erreur est survenue pendant l'installation!$\r$\nVeuillez consulter le fichier log '$INSTDIR\install_ui.log' pour plus d'informations."
LangString VBOX_ERROR_OPEN_LINK ${LANG_FRENCH}                      "Impossible d'ouvrir le lien dans le navigateur par d�faut."

LangString VBOX_UNINST_CONFIRM ${LANG_FRENCH}                       "Voulez-vous vraiment d�sinstaller $(^Name)?"
LangString VBOX_UNINST_SUCCESS ${LANG_FRENCH}                       "$(^Name) ont �t� d�sinstall�s."
LangString VBOX_UNINST_INVALID_D3D ${LANG_FRENCH}                   "Installation incorrecte du support Direct3D detect�e; une d�sinstallation ne sera pas tent�e."
LangString VBOX_UNINST_UNABLE_TO_RESTORE_D3D ${LANG_FRENCH}         "La restauration des fichiers originaux Direct3D a echou�. Veuillez r�installer DirectX."
