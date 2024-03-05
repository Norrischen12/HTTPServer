#include "./WordIndex.h"
#include <bits/stdc++.h>

namespace searchserver {

WordIndex::WordIndex() {
  // TODO: implement
  num_words_ = 0;
}

size_t WordIndex::num_words() {
  // TODO: implement
  return unique_words_.size();
}

void WordIndex::record(const string& word, const string& doc_name) {
    // TODO: implement
    if (umap_.find(doc_name) == umap_.end()){
      unordered_map<string, int> new_map;
      umap_.insert({doc_name, new_map});
      umap_[doc_name].insert({word, 1});
      unique_words_.insert(word);
    } else {
      if (umap_[doc_name].find(word) == umap_[doc_name].end()){
        umap_[doc_name][word] = 1;
        unique_words_.insert(word);
      } else{
        umap_[doc_name][word]++;
      }
    }
}

vector<Result> WordIndex::lookup_word(const string& word) {
    vector<Result> result;
    // TODO: implement
    for (auto i: umap_){
      if (i.second.find(word) != i.second.end()){
        Result res = Result(i.first, i.second[word]);
        result.push_back(res);
      }
    }

    sort(result.begin(), result.end(), [](const Result& a, const Result& b) {
        return a < b;
    });
    return result;
}

vector<Result> WordIndex::lookup_query(const vector<string>& query) {
  vector<Result> results;

  // TODO: implement
  for (auto i : umap_) {
    int sum = 0;
    unsigned int count = 0;
    for (auto j : query) {
      if (i.second.find(j) != i.second.end()){
        sum += i.second[j];
        count++;
      }
    }
    if (sum != 0 && count == query.size()){
      Result res = Result(i.first, sum);
      results.push_back(res);
    }
  }
    sort(results.begin(), results.end(), [](const Result& a, const Result& b) {
        return a < b;
    });
    return results;
}

}  // namespace searchserver
