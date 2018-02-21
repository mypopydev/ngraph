/*******************************************************************************
* Copyright 2017-2018 Intel Corporation
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*******************************************************************************/

#include <algorithm>

#include "ngraph/runtime/cpu/cpu_call_frame.hpp"
#include "ngraph/runtime/cpu/cpu_external_function.hpp"
#include "ngraph/runtime/cpu/cpu_tensor_view.hpp"
#include "ngraph/runtime/cpu/cpu_tracing.hpp"

using namespace std;
using namespace ngraph;

runtime::cpu::CPUCallFrame::CPUCallFrame(std::shared_ptr<CPUExternalFunction> external_function,
                                           EntryPoint compiled_function)
    : m_external_function(external_function)
    , m_compiled_function(compiled_function)
{
    setup_runtime_context();
}

runtime::cpu::CPUCallFrame::~CPUCallFrame()
{
    cleanup_runtime_context();
}

void runtime::cpu::CPUCallFrame::tensor_call(
    const std::vector<std::shared_ptr<ngraph::runtime::TensorView>>& input_tvs,
    const std::vector<std::shared_ptr<ngraph::runtime::TensorView>>& output_tvs)
{
    vector<void*> inputs;
    vector<void*> outputs;

    propagate_layouts(input_tvs, m_external_function->get_parameter_layout_descriptors());
    propagate_layouts(output_tvs, m_external_function->get_result_layout_descriptors());

    for (size_t i = 0; i < input_tvs.size(); i++)
    {
        shared_ptr<runtime::cpu::CPUTensorView> tv =
            static_pointer_cast<runtime::cpu::CPUTensorView>(input_tvs[i]);
        inputs.push_back(tv->get_data_ptr());
    }
    for (size_t i = 0; i < output_tvs.size(); i++)
    {
        shared_ptr<runtime::cpu::CPUTensorView> tv =
            static_pointer_cast<runtime::cpu::CPUTensorView>(output_tvs[i]);
        outputs.push_back(tv->get_data_ptr());
    }

    // Invoke compiled computation
    m_compiled_function(inputs.data(), outputs.data(), m_ctx);

    if (runtime::cpu::is_tracing_enabled())
    {
        generate_timeline(m_external_function->get_op_attrs(), m_ctx->op_durations);
    }
}

void runtime::cpu::CPUCallFrame::call(
    const std::vector<std::shared_ptr<runtime::TensorView>>& arguments,
    const std::vector<std::shared_ptr<runtime::TensorView>>& results)
{
    // TODO: Check types of args and result
    vector<shared_ptr<runtime::TensorView>> inputs;
    for (shared_ptr<runtime::TensorView> argument : arguments)
    {
        argument->collect_tensor_views(inputs, argument);
    }

    vector<shared_ptr<runtime::TensorView>> outputs;
    for (shared_ptr<runtime::TensorView> result : results)
    {
        result->collect_tensor_views(outputs, result);
    }

    tensor_call(inputs, outputs);
}

void runtime::cpu::CPUCallFrame::propagate_layouts(
    const std::vector<std::shared_ptr<runtime::TensorView>>& tvs,
    const LayoutDescriptors& layouts) const
{
    if (layouts.size() != tvs.size())
    {
        throw ngraph_error(
            "Error propagating layouts - tensor view and layout descriptor counts do not match");
    }
    for (size_t i = 0; i < tvs.size(); i++)
    {
        if (layouts[i] == nullptr)
        {
            throw ngraph_error(
                "Error propagating layouts - layout information missing from tensor view");
        }
        tvs[i]->get_descriptor()->set_tensor_view_layout(layouts[i]);
    }
}

vector<runtime::PerformanceCounter> runtime::cpu::CPUCallFrame::get_performance_data() const
{
    vector<runtime::PerformanceCounter> rc;
    auto* engine = m_external_function->m_execution_engine.get();
    if (engine)
    {
        auto get_count = engine->find_function<size_t()>("get_debug_timer_count");
        auto get_name = engine->find_function<const char*(size_t)>("get_debug_timer_name");
        auto get_microseconds =
            engine->find_function<size_t(size_t)>("get_debug_timer_microseconds");
        auto get_call_count = engine->find_function<size_t(size_t)>("get_debug_timer_call_count");

        if (get_count && get_name && get_microseconds && get_call_count)
        {
            size_t count = get_count();
            for (size_t i = 0; i < count; i++)
            {
                rc.push_back({get_name(i), get_microseconds(i), get_call_count(i)});
            }
        }
    }
    return rc;
}

void runtime::cpu::CPUCallFrame::setup_runtime_context()
{
    m_ctx = new CPURuntimeContext;

    m_ctx->op_durations = nullptr;
    if (runtime::cpu::is_tracing_enabled())
    {
        m_ctx->op_durations = new int64_t[m_external_function->get_op_attrs().size()];
    }
    const auto& mkldnn_emitter = m_external_function->get_mkldnn_emitter();
    m_ctx->mkldnn_primitives = mkldnn_emitter->get_mkldnn_primitives().data();
}

void runtime::cpu::CPUCallFrame::cleanup_runtime_context()
{
    delete[] m_ctx->op_durations;
    delete m_ctx;
}
