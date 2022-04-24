//
// Created by yangy on 8/19/2021.
//

#include <chocopy_optimization.hpp>

namespace lightir {

/* The Optimization Pass Your Code Here */

void PassManager::run(bool print_ir) {
    auto i = 0;
    for (auto pass : passes_) {
        i++;
        pass->run();
        if (print_ir || pass->isPrintIR()) {
            LOG(ERROR) << ">>>>>>>>>>>> After pass " << pass->getName() << " <<<<<<<<<<<<";
            m_->print();
        }
        if (pass->getName() == "Vectorization") {
            ((Vectorization *)pass)->vectorize_num = m_->vectorize_num;
        }
        if (pass->getName() == "Multithread") {
            ((Multithread *)pass)->thread_num = m_->thread_num;
        }
    }
}
} // namespace lightir