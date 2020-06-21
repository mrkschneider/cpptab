/*
 * util.hpp
 *
 *  Created on: Sep 26, 2019
 *      Author: mschneider
 */

#ifndef INCLUDE_ST_HPP_
#define INCLUDE_ST_HPP_

#include <string>
#include <vector>
#include <any>
#include <map>

namespace st {

	inline std::string char2string(char c){
		return std::string(1,c);
	}

	template <class T>
	std::vector<T> subVec(const std::vector<T>& xs, size_t start, size_t end){
			std::vector<T> result;
			for(size_t i=start;i<end;i++){
				result.push_back(xs[i]);
			}
			return result;
	}

	template <class T>
	inline T last(const std::vector<T>& xs){
			return xs[xs.size()-1];
	}

	inline char last(const std::string& s){
		return s[s.size()-1];
	}

	template <class K, class V>
	V getIn(const std::map<K,V>& m, const std::vector<K>& ks){
		auto m2 = m;
		size_t size = ks.size();
		for(size_t i=0;i<size-1;i++){
			m2 = std::any_cast<std::map<K,V>>(m2.at(ks[i]));
		}
		return m2.at(ks[size-1]);
	}

}


#endif /* INCLUDE_ST_HPP_ */
