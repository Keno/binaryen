/*
 * Copyright 2015 WebAssembly Community Group participants
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
 */

//
// Misc optimizations that are useful for and/or are only valid for
// emscripten output.
//

#include <asmjs/shared-constants.h>
#include <ir/import-utils.h>
#include <ir/localize.h>
#include <pass.h>
#include <shared-constants.h>
#include <wasm-builder.h>
#include <wasm.h>

namespace wasm {

namespace {

struct OptimizeCalls : public WalkerPass<PostWalker<OptimizeCalls>> {
  bool isFunctionParallel() override { return true; }

  Pass* create() override { return new OptimizeCalls; }

  void visitCall(Call* curr) {
    // special asm.js imports can be optimized
    auto* func = getModule()->getFunction(curr->target);
    if (!func->imported()) {
      return;
    }
    if (func->module == GLOBAL_MATH) {
      if (func->base == POW) {
        if (auto* exponent = curr->operands[1]->dynCast<Const>()) {
          if (exponent->value == Literal(double(2.0))) {
            // This is just a square operation, do a multiply
            Localizer localizer(curr->operands[0], getFunction(), getModule());
            Builder builder(*getModule());
            replaceCurrent(builder.makeBinary(
              MulFloat64,
              localizer.expr,
              builder.makeLocalGet(localizer.index, localizer.expr->type)));
          } else if (exponent->value == Literal(double(0.5))) {
            // This is just a square root operation
            replaceCurrent(
              Builder(*getModule()).makeUnary(SqrtFloat64, curr->operands[0]));
          }
        }
      }
    }
  }
};

} // namespace

struct PostEmscripten : public Pass {
  void run(PassRunner* runner, Module* module) override {
    // Apply the sbrk ptr, if it was provided.
    auto sbrkPtrStr =
      runner->options.getArgumentOrDefault("emscripten-sbrk-ptr", "");
    if (sbrkPtrStr != "") {
      auto sbrkPtr = std::stoi(sbrkPtrStr);
      ImportInfo imports(*module);
      auto* func = imports.getImportedFunction(ENV, "emscripten_get_sbrk_ptr");
      if (func) {
        Builder builder(*module);
        func->body = builder.makeConst(Literal(int32_t(sbrkPtr)));
        func->module = func->base = Name();
      }
    }

    // Optimize calls
    OptimizeCalls().run(runner, module);
  }
};

Pass* createPostEmscriptenPass() { return new PostEmscripten(); }

} // namespace wasm
