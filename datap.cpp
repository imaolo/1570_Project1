#include <bits/stdc++.h>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <iterator>
#include <experimental/filesystem>
#include <pthread.h>

namespace fs = std::experimental::filesystem;
using namespace std;

#define NUM_THREADS 5
#define NOISES_FN "noises.txt"
#define DELIMITERS_FN "delimiters.txt"
#define OUTPUT_FN "datap_output.txt"
#define stringLower(line) transform(line.begin(), line.end(), line.begin(), ::tolower);

vector<string> getLinesFrom(string);
vector<string> getInputPaths (const string&);
vector<string> readFiles(const vector<string>&);
vector<string> tokenize(vector<string>, string);
vector<string> removeNoises(vector<string>, vector<string>);
vector<pair<string,int>> countTokens(vector<string>);
vector<pair<string,int>> getTopTokens(vector<pair<string, int>>);
void writeToFile(string, vector<pair<string, int>>);
vector<int> getFileSizes(vector<string>);
bool sortbysecdesc(const pair<string, int>&, const pair<string,int>&);


struct ArgStruct {
  int id;
  vector<string> paths;
  vector<string> noises;
  vector<string> delimiters;
  vector<pair<string,int>> token_counts;
};
void *datap_thread(void *props){
  ArgStruct *args = (ArgStruct *)props;
  // 4 - read files by line
  vector<string> tokens = readFiles(args->paths);
  // 5 - tokenize
  for (string del: args -> delimiters)
    tokens = tokenize(tokens, del);
  // 6 - remove noise
  tokens = removeNoises(tokens, args->noises);
  // 7 - count tokens
  args->token_counts = countTokens(tokens);
}


/*
pipeline
1 - read delimiters file
2 - read noises file
3 - get file paths
LOAD BALANCE
4 - P - read files by line
5 - P - tokenize
6 - P - remove noise
7 - P - count remaining tokens
8 - extract top 10% of token
9 - write top tokens to output file
 */
int main(){
  // 1 - read delimiters files
  vector<string> delimiters = getLinesFrom(DELIMITERS_FN);
  // 2 - read noises file
  vector<string> noises = getLinesFrom(NOISES_FN);
  // side step - add delimiters to noises that are not already there
  for (string del: delimiters){
    if(!count(noises.begin(), noises.end(), del))
       noises.push_back(del);
  }
  // 3 - path_to_inputs is the path to the input files
  // 3 - input_paths contains the paths to all input files
  string path_to_inputs = fs::current_path().append("/input");
  vector<string> input_paths = getInputPaths(path_to_inputs);
  //LOAD BALANCE
  //get files sizes and find sum 
  vector<int> file_sizes = getFileSizes(input_paths);
  int message_size = 0;
  for (int size: file_sizes)
    message_size+=size;
  int size_per_thread = message_size/NUM_THREADS;
  //configure threads
  pthread_t ids[NUM_THREADS];
  ArgStruct *args = new ArgStruct[NUM_THREADS];
  int curr_file = 0;
  for (int i=0; i< NUM_THREADS; i++){
    int accumulated_size = 0;
    vector<string> arg_paths;
    while (accumulated_size < size_per_thread && curr_file < file_sizes.size()){
      arg_paths.push_back(input_paths[curr_file]);
      accumulated_size += file_sizes[curr_file];
      curr_file++;
    }
    if (i == NUM_THREADS - 1 && curr_file < file_sizes.size()){
      for (curr_file; curr_file<file_sizes.size(); curr_file++)
	arg_paths. push_back(input_paths[curr_file]);
    }
    args[i].id = i;
    args[i].noises = noises;
    args[i].delimiters = delimiters;
    args[i].paths = arg_paths;
    pthread_create(&ids[i],NULL, datap_thread,(void *) &args[i]);
  }
  for (int i=0;i< NUM_THREADS; i++)
    pthread_join(ids[i], NULL);
  //side step - add the counted tokens from each thread into one vector of pairs. sort the resultant vector of pairs
  vector<pair<string,int>> token_counts;
  vector<pair<string,int>>::iterator it;
  for (int i=0; i<NUM_THREADS; i++){
    for (it = args[i].token_counts.begin(); it != args[i].token_counts.end(); ++it){
      vector<pair<string, int>>::iterator has_token;
      has_token = find_if(token_counts.begin(), token_counts.end(), [&](const pair<string, int>& elem) {return elem.first == it->first;});
      if (has_token != token_counts.end())
	has_token->second += it->second;
      else
	token_counts.push_back( make_pair(it->first, it->second));
    }
  }
  sort(token_counts.begin(), token_counts.end(), sortbysecdesc);
  // 8 - extract top 10% tokens
  vector<pair<string, int>> top_token_counts = getTopTokens(token_counts);
  // 9 - write top tokens to output file
  writeToFile(OUTPUT_FN, top_token_counts);

  delete[] args;
  return 0;
}


//functions
void writeToFile(string fn, vector<pair<string, int>> token_counts){
  ofstream file (fn);
  if (file.is_open()){
    vector<pair<string,int>>::iterator it;
    for (it = token_counts.begin(); it != token_counts.end(); ++it)
      file << it->first << ":" << it->second << endl;
    file.close();
  }

}
vector<pair<string, int>> getTopTokens(vector<pair<string, int>> token_counts){
  vector<pair<string, int>> top_token_counts;
  int sum = 0;
  vector<pair<string, int>>::iterator it;
  for (it = token_counts.begin(); it != token_counts.end(); ++it)
    sum += it->second;
  int tmp_sum = 0;
  for (it = token_counts.begin(); it != token_counts.end(); ++it){
    tmp_sum += it->second;
    float percentage = (float) tmp_sum / (float) sum;
    if (percentage < 0.1)
      top_token_counts.push_back(*it);
    else
      break;
  }
  return top_token_counts;
}
vector<string> getLinesFrom(string fn){
  vector<string> lines;
  ifstream file (fn);
  string line;
  if (file.is_open()){
    while(getline(file,line)){
      lines.push_back(line);
    }
  }
  file.close();
  return lines;
}
//countTokens helper function
bool sortbysecdesc(const pair<string, int> &a, const pair<string, int> &b){
  return a.second > b.second;
}
vector<pair<string,int>> countTokens(vector<string> tokens){
  vector<pair<string,int>> token_counts;
  for (string token : tokens){
    vector<pair<string,int>>::iterator it;
    it = find_if(token_counts.begin(), token_counts.end(), [&](const pair<string,int>& elem){return elem.first == token;});
    if (it != token_counts.end())
      it->second++;
    else
      token_counts.push_back(make_pair(token, 1));
  }
  sort(token_counts.begin(), token_counts.end(), sortbysecdesc);
  return token_counts;
}
vector<string> removeNoises(vector<string> tokens, vector<string> noises){
  for (string noise: noises){
    vector<string>::iterator it = tokens.begin();
    while (it != tokens.end()){
      if (*it == noise)
        it = tokens.erase(it);
      else
        it++;
    }
  }
  return tokens;
}
vector<string> readFiles(const vector<string>& paths){
  vector<string> lines;
  ifstream file;
  string line;
  for (string path: paths){
    file.open(path);
    while(getline(file,line)){
        stringLower(line);
        lines.push_back(line);
    }
    file.close();
  }
  return lines;
}
vector<string> tokenize(vector<string> strings, string del){
  vector<string> tokens;
  for (string s: strings){
    int start = 0;
    int end = s.find(del);
    while (end != -1){
      tokens.push_back(s.substr(start, end - start));
      start = end + del.size();
      end = s.find(del, start);
    }
    tokens.push_back(s.substr(start, end - start));
  }
  return tokens;
}
vector<string> getInputPaths(const string& path){
  vector<string> paths;
  for (const auto & entry: fs::directory_iterator(path))
    paths.push_back(entry.path());
  return paths;
}

vector<int> getFileSizes(vector<string> paths){
  ifstream file;
  vector<int> sizes;
  for (string path: paths){
    file.open(path);
    if (file.is_open()){
      file.seekg(0,file.end);
      sizes.push_back(file.tellg());
      file.seekg(0,file.beg);
      file.close();
    }
  }
  return sizes;
}
//g++ main.cpp -o main -lstdc++fs
