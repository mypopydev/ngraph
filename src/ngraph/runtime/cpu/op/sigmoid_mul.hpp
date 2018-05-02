/*******************************************************************************
* Copyright 2018 Intel Corporation
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

#pragma once

#include "ngraph/op/util/requires_tensor_view_args.hpp"
#include "ngraph/util.hpp"

namespace ngraph
{
    namespace op
    {
        class SigmoidMultiply : public util::RequiresTensorViewArgs
        {
        public:
            enum class FunctionType {
                Sigmoid,
                Tanh
            };
            SigmoidMultiply(std::shared_ptr<Node> input_1, std::shared_ptr<Node> input_2);
            virtual std::shared_ptr<Node> copy_with_new_args(const NodeVector& new_args) const override;
            virtual void generate_adjoints(autodiff::Adjoints& adjoints,
                                           const NodeVector& deltas) override;
            FunctionType get_input_1_func_type() const { return input_1_type; }
            FunctionType get_input_2_func_type() const { return input_2_type; }
        private:
            FunctionType input_1_type;
            FunctionType input_2_type;
        };
    }
}