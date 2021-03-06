/*
 * Copyrights (c):
 *     2000 - 2008 , Werner Freytag.
 *     2009, Haiku
 * Distributed under the terms of the MIT License.
 *
 * Original Author:
 *              Werner Freytag <freytag@gmx.de>
 */

#include "PecoApp.h"

#include <Alert.h>
#include <Application.h>
#include <Bitmap.h>
#include <Catalog.h>
#include <ListView.h>
#include <MenuField.h>
#include <MenuItem.h>
#include <NodeInfo.h>
#include <String.h>
#include <StringView.h>
#include <TextControl.h>
#include <FilePanel.h>
#include <Volume.h>

#include <malloc.h>
#include <string.h>

#include "constants.h"
#include "functions.h"

#include "About.h"
#include "Fenster.h"
#include "Renamer_SearchReplace.h"
#include "Renamer_Extension.h"
#include "Renamer_Numbering.h"
#include "Renamer_InsertReplace.h"
#include "Renamer_UpperLower.h"
#include "Renamer_Remove.h"


#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "PecoApp"

PecoApp::PecoApp() : BApplication("application/x-vnd.pecora-PecoRename") {

	fRenameMode	= 0;
	fPfad		= NULL;
	fList		= new BList();

	fRenamers[0] = new Renamer_SearchReplace();
	fRenamers[1] = new Renamer_InsertReplace();
	fRenamers[2] = new Renamer_Remove();
	fRenamers[3] = new Renamer_Numbering();
	fRenamers[4] = new Renamer_Extension();
	fRenamers[5] = new Renamer_UpperLower();
};

void PecoApp::ReadyToRun() {

	fWindow = new Fenster();
	fListView	= (FileListView *)fWindow->FindView("fileListView");
	fStatusBar	= (BStatusBar *)fWindow->FindView("statusBar");

	UpdateWindowStatus();

	fFilePanel = new BFilePanel(B_OPEN_PANEL, NULL, NULL, B_FILE_NODE|B_DIRECTORY_NODE);
	fFilePanel->SetButtonLabel(B_DEFAULT_BUTTON, B_TRANSLATE("Select"));
	fFilePanel->SetButtonLabel(B_CANCEL_BUTTON, B_TRANSLATE("Cancel"));
	fFilePanel->Window()->SetTitle(B_TRANSLATE("Select files for renaming"));

	fScriptFilePanel = new BFilePanel(B_SAVE_PANEL, NULL, NULL, B_FILE_NODE, false,
		0, 0, true);
	fScriptFilePanel->SetButtonLabel(B_DEFAULT_BUTTON, B_TRANSLATE("Ok"));
	fScriptFilePanel->SetButtonLabel(B_CANCEL_BUTTON, B_TRANSLATE("Cancel"));
	fScriptFilePanel->Window()->SetTitle(B_TRANSLATE("Create shell script..."));

	fWindow->Show();

};

void PecoApp::AboutRequested() {

	const char	*CopyrightTexte[10];
	CopyrightTexte[0] = B_TRANSLATE("Copyright ©2000 by Werner Freytag");
	CopyrightTexte[1] = B_TRANSLATE("This software is freeware.");
	CopyrightTexte[2] = "";
	CopyrightTexte[3] = B_TRANSLATE("Special thanks to Sci for his help with the English translation!");
	CopyrightTexte[4] = NULL;
	
	About(B_TRANSLATE("About PecoRename"), B_TRANSLATE_SYSTEM_NAME("PecoRename"), B_TRANSLATE("Release 1.5"), CopyrightTexte, B_TRANSLATE("Thank you :-)"), "http://www.pecora.de/pecorename/");
}

bool PecoApp::QuitRequested() {
	return true;
}

void PecoApp::MessageReceived(BMessage *msg) {
	switch( msg->what ) {
		case B_SIMPLE_DATA:			RefsReceived (msg); break;
		case MSG_MENU_NEW:			New(); break;
		case MSG_SELECT_FILES: 		fFilePanel->Show(); break;
		case MSG_DO_IT: 			/*MakeList();*/ DoIt(); break; // Use the current List
		case MSG_RENAME_SETTINGS:	MakeList(); break;
		case MSG_MENU_SCRIPT:		if (!NothingToDo()) fScriptFilePanel->Show(); break;
		case B_SAVE_REQUESTED:		CreateScript(msg); break;
		
	   	default:
	   		BApplication::MessageReceived(msg);
	}
};

void PecoApp::RefsReceived ( BMessage* msg ) {

	entry_ref	ref;
	BPath		aPath;
	BEntry		aEntry;
	off_t		size;
	time_t		timer;
	
	fWindow->Lock();
	BStringView* 	pfadView 	= (BStringView *)fWindow->FindView("pfadView");
	fWindow->Unlock();
	
	//Pfad finden
	for ( int i=0; msg->FindRef("refs", i, &ref) == B_OK; i++ ) if ( ref.device > 1 ) break;

	BVolume volume(ref.device);
	if (volume.IsReadOnly()) {
	   BAlert  *myAlert = new BAlert(NULL, B_TRANSLATE("The volume is read only you cannot rename the files"), B_TRANSLATE("OK"));
	   myAlert->Go();
	   return;
	}
	// TODO Check the directory

	fWindow->Lock();
	BButton* ChooseButton = (BButton *) fWindow->FindView("selectFiles");
	ChooseButton->SetFlat(true);
	fWindow->Unlock();

	if ( ref.device > 1 ) {
		New();

		aEntry = BEntry(&ref);
		BPath( &aEntry ).GetParent(&fPfad);
				
		fWindow->Lock();
		pfadView->SetText( fPfad.Path() );
		fWindow->Unlock();
		
		//zählen
		type_code	typeFound;
		int32		total = 0;
		msg->GetInfo("refs", &typeFound, &total);
		
		fWindow->Lock();
		fStatusBar->SetText("");
		fStatusBar->Show();
		fStatusBar->SetMaxValue( total );
		fWindow->Unlock();
		
		BPath	newPath;
		
		bool	didntshow_msgmultidir = true;
		
		for ( int i=0; msg->FindRef("refs", i, &ref) == B_OK; i++ ) {
			
			fWindow->Lock();
			fStatusBar->Update(1);
			fWindow->Unlock();
			
			// Laufwerke ausfiltern
			if ( ref.device == 1 ) continue;
 			
			// Dateien mit falschem Pfad ausfiltern
			aEntry = BEntry(&ref);
			aPath = BPath(&aEntry);
			
			BPath( &aEntry ).GetParent(&newPath);
			
			if ( (strcmp( fPfad.Path(), newPath.Path() ) != 0 ) ) {
				if ( didntshow_msgmultidir ) {
					BAlert*	myAlert = new BAlert(NULL, B_TRANSLATE("I'm sorry, but I can't rename files from different directories.\n\nOnly the files in the first found directory will be imported!"), B_TRANSLATE("Ok"));
					myAlert->Go();
					didntshow_msgmultidir = false;
				}
				continue;
			}
			
			// Werte auslesen
			if (aEntry.IsFile()) aEntry.GetSize(&size); 
			else
				if (aEntry.IsSymLink()) size = -1;
				else
					if (aEntry.IsDirectory()) size = -2;
					else continue;
			
			aEntry.GetModificationTime(&timer);
			fList->AddItem(new FileListItem(aPath.Leaf(), size, timer, &ref));
			
		}
		
		fWindow->Lock();
		fListView->AddList(fList);
		fStatusBar->Hide();
		fStatusBar->SetMaxValue(fList->CountItems());
		fStatusBar->Reset("");
		fWindow->Unlock();
		
		MakeList();
	}
	fWindow->Activate();
	UpdateWindowStatus();
}

void PecoApp::New() {
	
	fWindow->Lock();
	
	fListView->Clear();
	
	BStringView* 	pfadView = (BStringView *)fWindow->FindView("pfadView");
	pfadView->SetText(NULL);
	
	fWindow->Unlock();
	
	fList->MakeEmpty(); // TODO This can be remove is redundance of fListView
	
	UpdateWindowStatus();
}

bool PecoApp::NothingToDo() {

	FileListItem	*ListItem;
	
	bool nothing_to_do = true;

	for (int32 i = 0; (ListItem = (FileListItem *)fListView->ItemAt(i)) != NULL; i++ )
		if (ListItem->fNewName.Length() > 0 ) { nothing_to_do = false; break; }

	return	nothing_to_do;

}

void PecoApp::CreateScript(BMessage *msg) {
	entry_ref	directory;
	const char	*name;
	msg->FindRef("directory", &directory);
	msg->FindString("name", &name);
	
	BPath path(&directory);
	path.Append(name);
	
	BFile file(path.Path(), B_READ_WRITE | B_CREATE_FILE | B_ERASE_FILE);

	// Initiate
	fWindow->Lock();
	fStatusBar->Show();
	fStatusBar->Reset("");
	fStatusBar->SetMaxValue(fList->CountItems());

	BStringView* 	pfadView = (BStringView *)fWindow->FindView("pfadView");
	BString			Pfad(pfadView->Text());
	fWindow->Unlock();

	FileListItem	*ListItem;
	BString			AlterName, NeuerName;
	BString			output = 
					"#!/bin/sh\n"
					"\n"
					"# This script was created with PecoRename by Werner Freytag.\n"
					"# Visit http://www.pecora.de/pecorename for details!\n\n"
					"alert \"This script will rename files.\n\n"
					"Do you really want to continue?\" \"Yes\" \"No\"\n\n"
					"if [ $? -ne 0 ]\n"
					"then\n"
					"	exit 0\n"
					"fi\n\n";
	
	for (int32 i = 0; (ListItem = (FileListItem *)fListView->ItemAt(i)) != NULL; i++ ) {
		fWindow->Lock();
		fStatusBar->Update(1);
		fWindow->Unlock();
		if (ListItem->fNewName.String() != "" ) {
			AlterName = Pfad; AlterName.Append("/").Append(ListItem->fName);
			AlterName.Replace("\"", "\\\"", AlterName.Length());
			NeuerName = Pfad; NeuerName.Append("/").Append(ListItem->fNewName);
			NeuerName.Replace("\"", "\\\"", NeuerName.Length());
			output << "mv \"" << AlterName.String() << "\" \"" << NeuerName.String() << "\"\n";
		}
	}
	
	file.Write(output.String(), output.Length());
	
	mode_t	perms;
	
	file.GetPermissions(&perms);
	file.SetPermissions(perms | S_IXUSR | S_IXGRP | S_IXOTH );
	
	BNode		node(file);
	BNodeInfo	node_info(&node);
	node_info.SetType("text/plain");
	
	fWindow->Lock();
	fStatusBar->Hide();
	fWindow->Unlock();

}

void PecoApp::DoIt() {

	if (NothingToDo()) return;

	fWindow->Lock();
	fStatusBar->SetText("");
	fStatusBar->Show();
	fStatusBar->SetMaxValue(fList->CountItems());

	BButton		*okButton = (BButton *)fWindow->FindView("DoIt");
	okButton->SetEnabled(false);
	
	BStringView* 	pfadView = (BStringView *)fWindow->FindView("pfadView");
	BString	Pfad(pfadView->Text());
	fWindow->Unlock();
	
	BString	AlterName, NeuerName;
	BEntry	Datei;
	FileListItem    *ListItem;
	for (int32 i = 0; (ListItem = (FileListItem *)fListView->ItemAt(i)) != NULL; i++ ) {
		fWindow->Lock();
		fStatusBar->Update(1);
		fWindow->Unlock();
		if (ListItem->fNewName != "" && ListItem->Error() == 0) {
			AlterName = Pfad; AlterName.Append("/").Append(ListItem->fName);
			NeuerName = Pfad; NeuerName.Append("/").Append(ListItem->fNewName);
			Datei.SetTo(AlterName.String());
			status_t result;
			if ( (result = Datei.Rename(NeuerName.String())) != B_OK ) {
				ListItem->SetError(1);
				fWindow->Lock();
				fListView->InvalidateItem(i);
				fWindow->Unlock();
			} else {
				fWindow->Lock();
				ListItem->SetName(ListItem->fNewName);
				ListItem->SetNewName("");
				fListView->InvalidateItem(i);
				fWindow->Unlock();

			}
		}
	}

	fWindow->Lock();
	fStatusBar->Reset("");
	fStatusBar->Hide();
	fWindow->Unlock();

	bool noerror = true;

	for (int32 i = 0; (ListItem = (FileListItem *)fListView->ItemAt(i)) != NULL; i++ ) {
		if (ListItem->fErrorStatus == 1 ) {
				noerror = false;
			break;
		}
	}

	if (noerror) MakeList();
	else {
		BAlert	*myAlert	= new BAlert(NULL, B_TRANSLATE("I've marked the files that caused the errors in red!"), B_TRANSLATE("Ok"));
		myAlert->Go();
	}
	
}
