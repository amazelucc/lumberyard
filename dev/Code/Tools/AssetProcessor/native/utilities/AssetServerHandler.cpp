/*
* All or portions of this file Copyright (c) Amazon.com, Inc. or its affiliates or
* its licensors.
*
* For complete copyright and license terms please see the LICENSE at the root of this
* distribution (the "License"). All use of this software is governed by the License,
* or, if provided, by the license below or the license accompanying this file. Do not
* remove or modify any license notices. This file is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*
*/

#include <native/utilities/AssetServerHandler.h>
#include <native/resourcecompiler/rcjob.h>
#include <native/utilities/assetUtils.h>
#include <AzToolsFramework/Archive/ArchiveAPI.h>
#include <QDir>

namespace AssetProcessor
{

    QString ComputeArchiveFilePath(const AssetProcessor::BuilderParams& builderParams)
    {
        QFileInfo fileInfo(builderParams.m_processJobRequest.m_sourceFile.c_str());
        QString assetServerAddress = QDir::toNativeSeparators(AssetUtilities::ServerAddress());
        if (!assetServerAddress.isEmpty())
        {
            QDir assetServerDir(assetServerAddress);
            QString archiveFileName = builderParams.GetServerKey() + ".zip";
            QString archiveFilePath = QDir(assetServerDir.filePath(fileInfo.path())).filePath(archiveFileName);
            return archiveFilePath;
        }

        return QString();
    }

    AssetServerHandler::AssetServerHandler()
    {
        AssetServerBus::Handler::BusConnect();
    }

    AssetServerHandler::~AssetServerHandler()
    {
        AssetServerBus::Handler::BusDisconnect();
    }

    bool AssetServerHandler::IsServerAddressValid()
    {
        QString address = AssetUtilities::ServerAddress();
        bool isValid = !address.isEmpty() && QDir(address).exists();
        return isValid;
    }

    bool AssetServerHandler::RetrieveJobResult(const AssetProcessor::BuilderParams& builderParams)
    {
        AssetBuilderSDK::JobCancelListener jobCancelListener(builderParams.m_rcJob->GetJobEntry().m_jobRunKey);
        AssetUtilities::QuitListener listener;
        listener.BusConnect();

        QString archiveAbsFilePath = ComputeArchiveFilePath(builderParams);
        if (archiveAbsFilePath.isEmpty())
        {
            AZ_Error(AssetProcessor::DebugChannel, false, "Extracting archive operation failed. Archive Absolute Path is empty. \n");
            return false;
        }

        if (!QFile::exists(archiveAbsFilePath))
        {
            // file does not exist on the server 
            AZ_TracePrintf(AssetProcessor::DebugChannel, "Extracting archive operation cancelled. Archive does not exist on server. \n");
            return false;
        }

        if (listener.WasQuitRequested() || jobCancelListener.IsCancelled())
        {
            AZ_TracePrintf(AssetProcessor::DebugChannel, "Extracting archive operation cancelled. \n");
            return false;
        }
        AZ_TracePrintf(AssetProcessor::DebugChannel, "Extracting archive for job (%s, %s, %s) with fingerprint (%u).\n",
            builderParams.m_rcJob->GetJobEntry().m_pathRelativeToWatchFolder.toUtf8().data(), builderParams.m_rcJob->GetJobKey().toUtf8().data(),
            builderParams.m_rcJob->GetPlatformInfo().m_identifier.c_str(), builderParams.m_rcJob->GetOriginalFingerprint());
        bool success = false;
        AzToolsFramework::ArchiveCommands::Bus::BroadcastResult(success, &AzToolsFramework::ArchiveCommands::ExtractArchiveBlocking, archiveAbsFilePath.toUtf8().data(), builderParams.GetTempJobDirectory(), false);
        AZ_Error(AssetProcessor::DebugChannel, success, "Extracting archive operation failed.\n");
        return success;
    }

    bool AssetServerHandler::StoreJobResult(const AssetProcessor::BuilderParams& builderParams, AZStd::vector<AZStd::string>& sourceFileList)
    {
        AssetBuilderSDK::JobCancelListener jobCancelListener(builderParams.m_rcJob->GetJobEntry().m_jobRunKey);
        AssetUtilities::QuitListener listener;
        listener.BusConnect();
        QString archiveAbsFilePath = ComputeArchiveFilePath(builderParams);
        
        if (archiveAbsFilePath.isEmpty())
        {
            AZ_Error(AssetProcessor::DebugChannel, false, "Creating archive operation failed. Archive Absolute Path is empty. \n");
            return false;
        }

        if (QFile::exists(archiveAbsFilePath))
        {
            // file already exists on the server 
            AZ_TracePrintf(AssetProcessor::DebugChannel, "Creating archive operation cancelled. An archive of this asset already exists on server. \n");
            return true;
        }
        
        if (listener.WasQuitRequested() || jobCancelListener.IsCancelled())
        {
            AZ_TracePrintf(AssetProcessor::DebugChannel, "Creating archive operation cancelled. \n");
            return false;
        }

        bool success = false;

        AZ_TracePrintf(AssetProcessor::DebugChannel, "Creating archive for job (%s, %s, %s) with fingerprint (%u).\n",
            builderParams.m_rcJob->GetJobEntry().m_pathRelativeToWatchFolder.toUtf8().data(), builderParams.m_rcJob->GetJobKey().toUtf8().data(),
            builderParams.m_rcJob->GetPlatformInfo().m_identifier.c_str(), builderParams.m_rcJob->GetOriginalFingerprint());
        AzToolsFramework::ArchiveCommands::Bus::BroadcastResult(success, &AzToolsFramework::ArchiveCommands::CreateArchiveBlocking, archiveAbsFilePath.toUtf8().data(), builderParams.GetTempJobDirectory());
        AZ_Error(AssetProcessor::DebugChannel, success, "Creating archive operation failed. \n");

        if (success && sourceFileList.size())
        {
            // Check if any of our output products for this job was a source file which would not be in the temp folder
            // If so add it to the archive
            AddSourceFilesToArchive(builderParams, archiveAbsFilePath, sourceFileList);
        }
        return success;
    }

    bool AssetServerHandler::AddSourceFilesToArchive(const AssetProcessor::BuilderParams& builderParams, const QString& archivePath, AZStd::vector<AZStd::string>& sourceFileList)
    {
        bool allSuccess{ true };
        for (const auto& thisProduct : sourceFileList)
        {
            QFileInfo sourceFile{ builderParams.m_rcJob->GetJobEntry().GetAbsoluteSourcePath() };
            QDir sourceDir{ sourceFile.absoluteDir() };

            if (!QFileInfo(sourceDir.absoluteFilePath(thisProduct.c_str())).exists())
            {
                AZ_Warning(AssetProcessor::DebugChannel, false, "Failed to add %s to %s - source does not exist in expected location (sourceDir %s )", thisProduct.c_str(), archivePath.toUtf8().data(), sourceDir.path().toUtf8().data());
                allSuccess = false;
                continue;
            }
            bool success{ false };
            AzToolsFramework::ArchiveCommands::Bus::BroadcastResult(success, &AzToolsFramework::ArchiveCommands::AddFileToArchiveBlocking, archivePath.toUtf8().data(), sourceDir.path().toUtf8().data(), thisProduct.c_str());
            if (!success)
            {
                AZ_Warning(AssetProcessor::DebugChannel, false, "Failed to add %s to %s", thisProduct.c_str(), archivePath.toUtf8().data());
                allSuccess = false;
            }

        }
        return allSuccess;
    }
}// AssetProcessor