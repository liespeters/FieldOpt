/******************************************************************************
   Created by einar on 3/27/17.
   Copyright (C) 2017 Einar J.M. Baumann <einar.baumann@gmail.com>

   This file is part of the FieldOpt project.

   FieldOpt is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   FieldOpt is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with FieldOpt.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/
#include "Utilities/math.hpp"
#include "RGARDD.h"

namespace Optimization {
namespace Optimizers {

RGARDD::RGARDD(Settings::Optimizer *settings,
               Optimization::Case *base_case,
               Model::Properties::VariablePropertyContainer *variables,
               Reservoir::Grid::Grid *grid) : GeneticAlgorithm(settings,
                                                               base_case,
                                                               variables,
                                                               grid) {
    assert(population_size_ % 2 == 0); // Need an even number of chromosomes
    discard_parameter_ = 1.0/population_size_;
    mating_pool_ = population_;
    stagnation_limit_ = 1e-10;
}
void RGARDD::iterate() {
    population_ = sortPopulation(population_);

    if (is_stagnant()) {
        if (verbosity_level_ > 1) {
            cout << "The population has stagnated in generation " << iteration_ << ". Repopulating." << endl;
        }
        repopulate();
        iteration_++;
        return;
    }

    mating_pool_ = selection(population_);
    double r;
    for (int i = 0; i < population_size_ / 2; ++i) {
        r = random_double(gen_);
        Chromosome p1 = mating_pool_[i]; // Parent 1
        Chromosome p2 = mating_pool_[population_size_ / 2 + i]; // Parent 2
        vector<Chromosome> offspring;
        if (r > p_crossover_ && p1.rea_vars != p2.rea_vars) {
            offspring = crossover(vector<Chromosome>{p1, p2});
        }
        else {
            offspring = mutate(vector<Chromosome>{p1, p2});
        }
        offspring[0].createNewCase();
        offspring[1].createNewCase();
        case_handler_->AddNewCase(offspring[0].case_pointer);
        case_handler_->AddNewCase(offspring[1].case_pointer);
        mating_pool_[i] = offspring[0];
        mating_pool_[population_size_ / 2 + i] = offspring[1];
    }
    iteration_++;
}
void RGARDD::handleEvaluatedCase(Case *c) {
    int index = -1;
    for (int i = 0; i < mating_pool_.size(); ++i) {
        if (mating_pool_[i].case_pointer == c)
            index = i;
    }
    if (isBetter(c, population_[index].case_pointer)) {
        population_[index] = mating_pool_[index];
        if (isImprovement(c)) {
            tentative_best_case_ = c;
            if (verbosity_level_ > 1) {
                cout << "New best in generation " << iteration_ << ": "
                     << tentative_best_case_->objective_function_value() << endl;
            }
        }
    }
}
vector<GeneticAlgorithm::Chromosome> RGARDD::selection(vector<Chromosome> population) {
    auto mating_pool = population_;
    int n_repl = floor(population_size_ * discard_parameter_);
    for (int i = population_size_-n_repl; i < population_size_; ++i) {
        mating_pool[i] = population_[i-population_size_+n_repl];
    }
    return sortPopulation(mating_pool);
}
vector<GeneticAlgorithm::Chromosome> RGARDD::crossover(vector<Chromosome> mating_pool) {
    assert(mating_pool.size() == 2);
    auto p1 = mating_pool[0];
    auto p2 = mating_pool[1];
    auto r = random_doubles(gen_, 0, 1, p1.rea_vars.size());
    Eigen::VectorXd dirs(r.size());
    for (int i = 0; i < r.size(); ++i) {
        if (r[i] < 0.5) {
            dirs(i) = 0;
        }
        else {
            dirs(i) = p1.rea_vars(i) - p2.rea_vars(i);
        }
    }
    Chromosome o1 = Chromosome(p1.case_pointer);
    Chromosome o2 = Chromosome(p2.case_pointer);

    double s = abs(p1.ofv() - p2.ofv()) /
        (population_[0].ofv() - population_[population_size_-1].ofv());
    o1.rea_vars = p1.rea_vars + s * dirs;
    o2.rea_vars = p2.rea_vars + s * dirs;
    return vector<Chromosome>{o1, o2};
}
vector<GeneticAlgorithm::Chromosome> RGARDD::mutate(vector<Chromosome> offspring) {
    double s = pow(1.0 - (iteration_*1.0/max_generations_), decay_rate_);
    auto p1 = offspring[0];
    auto p2 = offspring[1];
    auto o1 = Chromosome(p1);
    auto o2 = Chromosome(p2);
    Eigen::Map<Eigen::VectorXd> dir(
        random_doubles(gen_,
                       -mutation_strength_,
                       mutation_strength_,
                       p1.rea_vars.size()).data(),
        p1.rea_vars.size()
    );
    o1.rea_vars = p1.rea_vars + s * dir *(upper_bound_ - lower_bound_);
    o1.rea_vars = p2.rea_vars + s * dir *(upper_bound_ - lower_bound_);
    return population_;
}
bool RGARDD::is_stagnant() {
    // Using the sums of the variable values in each chromosome
    vector<double> sums;
    for (auto chrom : population_) {
        sums.push_back(chrom.rea_vars.sum());
    }
    double sum = std::accumulate(sums.begin(), sums.end(), 0.0);
    double mean = sum / sums.size();
    vector<double> diff(sums.size());
    transform(sums.begin(), sums.end(), diff.begin(),
              bind2nd(minus<double>(), mean)
    );
    double sq_sum = inner_product(diff.begin(), diff.end(), diff.begin(), 0.0);
    double stdev = sqrt(sq_sum / sums.size());

    return stdev <= stagnation_limit_;
}
void RGARDD::repopulate() {
    for (int i = 0; i < population_size_ - 1; ++i) {
        auto new_case = generateRandomCase();
        population_[i+1] = Chromosome(new_case);
        case_handler_->AddNewCase(new_case);
    }
    mating_pool_ = population_;
}

}
}
