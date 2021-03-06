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
#include <AWS_precompiled.h>
#include <Configuration/FlowNode_ConfigureProxy.h>

#include <CloudGemFramework/AwsApiJob.h>
#include <CloudGemFramework/CloudGemFrameworkBus.h>
#include <CloudCanvas/CloudCanvasIdentityBus.h>

namespace LmbrAWS
{
    FlowNode_ConfigureProxy::FlowNode_ConfigureProxy(IFlowNode::SActivationInfo* activationInfo)
        : BaseMaglevFlowNode< eNCT_Singleton >(activationInfo)
    {
    }

    Aws::Vector<SInputPortConfig> FlowNode_ConfigureProxy::GetInputPorts() const
    {
        static const Aws::Vector<SInputPortConfig> inputPorts = {
            InputPortConfig_Void("Configure", "Set the proxy configuration."),
            InputPortConfig<string>("Host", "Proxy host."),
            InputPortConfig<int>("Port", "Proxy port."),
            InputPortConfig<string>("UserName", "Proxy user name."),
            InputPortConfig<string>("Password", "Proxy password.")
        };

        return inputPorts;
    }

    Aws::Vector<SOutputPortConfig> FlowNode_ConfigureProxy::GetOutputPorts() const
    {
        static const Aws::Vector<SOutputPortConfig> outputPorts = {
        };
        return outputPorts;
    }

    void FlowNode_ConfigureProxy::ProcessEvent_Internal(EFlowEvent event, SActivationInfo* pActInfo)
    {
        if (event == eFE_Activate && IsPortActive(pActInfo, EIP_Configure))
        {
            CloudGemFramework::AwsApiJob::Config* defaultClientSettings{ nullptr };
            EBUS_EVENT_RESULT(defaultClientSettings, CloudGemFramework::CloudGemFrameworkRequestBus, GetDefaultConfig);
            if (defaultClientSettings)
            {
                defaultClientSettings->proxyHost = GetPortString(pActInfo, EIP_Host).c_str();
                defaultClientSettings->proxyPort = GetPortInt(pActInfo, EIP_Port);
                defaultClientSettings->proxyUserName = GetPortString(pActInfo, EIP_UserName).c_str();
                defaultClientSettings->proxyPassword = GetPortString(pActInfo, EIP_Password).c_str();
            }
            EBUS_EVENT(CloudGemFramework::CloudCanvasPlayerIdentityBus, ApplyConfiguration);

            ActivateOutput(pActInfo, EOP_Success, true);
        }
    }

    REGISTER_CLASS_TAG_AND_FLOW_NODE("AWS:Configuration:ConfigureProxy", FlowNode_ConfigureProxy);
} // namespace Amazon
