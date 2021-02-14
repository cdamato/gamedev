#ifndef BIT_TYPES_H
#define BIT_TYPES_H

#include <array>
#include <bitset>


template <typename element, size_t array_size>
class marked_storage {
private:
	std::bitset<array_size> _markers;
	std::array<element, array_size> _container;
public:

	constexpr bool exists(const size_t id) const { return _markers.test(id);}
	// Takes in an integer bitflag to check for multiple elements at once
	constexpr bool exists_all(const size_t bitflag) const {
		return (_markers & bitflag) == bitflag;
	}
	// To avoid extraneous testing, no validity checks are performed here
	constexpr element& get(const size_t id) { return _container[id]; }
	constexpr void remove(const size_t id) { _markers.set(id, false); }
	constexpr element& add(const size_t id, const element e) {
		_container[id] = e;
		_markers.set(id, true);
		return _container[id];
	}
	size_t size() { return _markers.count(); }
	class iterator;
	
	iterator begin() { return iterator(0, this); }
	iterator end() { return iterator(array_size, this); }


	class iterator {
	public:

 		iterator(size_t index, marked_storage* store): _index(index), _container(store) {
 			if (_index != array_size  && !_container->exists(_index)) jump_next_index();
		}
    	iterator operator++() {
    		jump_next_index();
    		return *this;
    	}

    	size_t index() { return _index; }
    	bool operator!=(const iterator & other) const { return _index != other._index; }
    	element& operator*() { return _container->get(_index); }
	private:
    	size_t _index;
    	marked_storage* _container;
		void jump_next_index() {
		    _index++;
            if (_index >= array_size) {
                _index = array_size;
                return;
            }
		    while (!_container->_markers.test(_index)) {
                auto new_bitset = _container->_markers;
                new_bitset >>= _index;
                new_bitset &= std::bitset<array_size>(0xFFFFFFFFFFFFFFFF);
                if (new_bitset.to_ullong() != 0) {
                    _index += __builtin_ctzll(new_bitset.to_ullong());
                } else {
                    _index += 64; //advance by a whole ullong. since this entire ullong is empty
                }
                if (_index >= array_size) {
                    _index = array_size;
                    return;
                }
            }
		}
    	friend class marked_storage;
	};
};

#endif //BIT_TYPES_H
