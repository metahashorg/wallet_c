function Component()
{
    // default constructor
}

function Controller()
{
}

Component.prototype.beginInstallation = function()
{
    if (installer.value("os") === "win") {
        installer.setValue("RunProgram", "@TargetDir@/MetaGate.exe");
    }
}

Component.prototype.createOperations = function()
{
    // call default implementation to actually install MetaGate.exe
    component.createOperations();

    if (systemInfo.productType === "windows") {
        component.addOperation("CreateShortcut", "@TargetDir@/MetaGate.exe", "@StartMenuDir@/MetaGate.lnk",
            "workingDirectory=@TargetDir@", "iconPath=@TargetDir@/MetaGate.exe",
            "iconId=0", "description=Launch Metahash Wallet");
        component.addOperation("CreateShortcut", "@TargetDir@/MetaGate.exe", "@DesktopDir@/MetaGate.lnk",
                "workingDirectory=@TargetDir@", "iconPath=@TargetDir@/MetaGate.exe",
                "iconId=0", "description=Launch Metahash Wallet");
    }

    //component.addElevatedOperation("Execute", "@TargetDir@\\AutoUpdater.exe", "-install", "UNDOEXECUTE", "@TargetDir@\\AutoUpdater.exe", "-remove");
    
    component.addElevatedOperation("GlobalConfig", "HKEY_CLASSES_ROOT\\mhpay", "Default", "URL:Mh pay protocol");
    component.addElevatedOperation("GlobalConfig", "HKEY_CLASSES_ROOT\\mhpay", "URL Protocol", "");
    component.addElevatedOperation("GlobalConfig", "HKEY_CLASSES_ROOT\\mhpay\\shell\\open\\command", "Default", "\"@TargetDir@\\MetaGate.exe\" \"%1\"");
    component.addElevatedOperation("GlobalConfig", "HKEY_CLASSES_ROOT\\mhpay\\DefaultIcon", "Default", "@TargetDir@\\MetaGate.exe,1");
}

Component.prototype.IntroductionPageCallback =function() {
 var result = QMessageBox.question("quit.question", "Start Program", "Do you want to start the installed application?",QMessageBox.Yes | QMessageBox.No);
   // if (installer.isUpdater()) {
         if (installer.isProcessRunning("MetaGate.exe")) {
             var widget =gui.currentPageWidget();
             widget.ErrorLabel.setText("<font color='red'>" + "Process: " + "YOUR_PROCESS" + " still running." + "</font>");

            // hide all following pages
installer.setDefaultPageVisible(QInstaller.TargetDirectory, false);
installer.setDefaultPageVisible(QInstaller.ComponentSelection, false);
installer.setDefaultPageVisible(QInstaller.ReadyForInstallation, false);
installer.setDefaultPageVisible(QInstaller.StartMenuSelection, false);
installer.setDefaultPageVisible(QInstaller.PerformInstallation, false);
installer.setDefaultPageVisible(QInstaller.LicenseCheck, false);
}
    // }
}


/*
function Controller()
{
    installer.installationFinished.connect(function() {

        var isUpdate = installer.isUpdater();

        if(isUpdate)
        {
             var targetDir = installer.value("TargetDir");
             console.log("targetDir: " + targetDir);
             //installer.executeDetached(targetDir+"/AutoUpdater.exe --install");

        }else{
            var result = QMessageBox.question("quit.question", "Start Program", "Do you want to start the installed application?",QMessageBox.Yes | QMessageBox.No);
            if( result == QMessageBox.Yes)
            {
                var targetDir = installer.value("TargetDir");
                console.log("targetDir: " + targetDir);
                console.log("Is Updater: " + installer.isUpdater());
                console.log("Is Uninstaller: " + installer.isUninstaller());
                console.log("Is Package Manager: " + installer.isPackageManager());
                //installer.executeDetached(targetDir+"/AutoUpdater.exe --install");
            }
        }
        });
    installer.updateFinished.connect(function(){
            var targetDir = installer.value("TargetDir");
            console.log("targetDir: " + targetDir);
            //installer.executeDetached(targetDir+"/AutoUpdater.exe --install");
    });
}
*/
