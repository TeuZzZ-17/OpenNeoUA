#ifndef LISTNODE_H_INCLUDED
#define LISTNODE_H_INCLUDED

#include <cstddef>
#include <deque>
#include <list>
#include <vector>

template <typename T> class RefList: protected std::list<T>
{
public:
    class Node;
    typedef std::vector<T> SafeCopy;

    // Reuses snapshot storage while keeping one independent buffer per nested
    // traversal. This preserves safe_iter() semantics when Update() recurses
    // through a BACT hierarchy, without allocating a fresh vector every time.
    class SnapshotWorkspace
    {
    public:
        class View
        {
        public:
            typedef typename SafeCopy::const_iterator const_iterator;

            View(View &&other) noexcept
            : _owner(other._owner), _depth(other._depth), _copy(other._copy)
            {
                other._owner = NULL;
                other._copy = NULL;
            }

            ~View()
            {
                if (_owner)
                    _owner->Release(_depth, _copy);
            }

            const_iterator begin() const { return _copy->begin(); }
            const_iterator end() const { return _copy->end(); }

            View(const View &) = delete;
            View &operator=(const View &) = delete;
            View &operator=(View &&) = delete;

        private:
            friend class SnapshotWorkspace;

            View(SnapshotWorkspace *owner, size_t depth, SafeCopy *copy)
            : _owner(owner), _depth(depth), _copy(copy)
            {}

            SnapshotWorkspace *_owner;
            size_t _depth;
            SafeCopy *_copy;
        };

        View Capture(const RefList &source)
        {
            const size_t depth = _depth;
            if (depth == _buffers.size())
                _buffers.emplace_back();

            SafeCopy &copy = _buffers[depth];
            copy.assign(source.begin(), source.end());
            ++_depth;
            return View(this, depth, &copy);
        }

    private:
        void Release(size_t depth, SafeCopy *copy)
        {
            // Views are scoped and therefore released in reverse capture order.
            // clear() keeps the high-water capacity for the next frame.
            if (_depth == depth + 1)
            {
                copy->clear();
                --_depth;
            }
        }

        std::deque<SafeCopy> _buffers;
        size_t _depth = 0;
    };

    typedef std::list<T> _T_Base;
    typedef RefList<T> _T_List;
    typedef typename _T_Base::iterator _T_interListIter;
    typedef Node& (* _T_RefNodeCallBack)(T&);

    class Node
    {
    friend class RefList;
    public:
        Node() : _pList(NULL) {};
        Node(_T_List *lst, _T_interListIter it)
        {
            _pList = lst;
            _it = it;
        };

        ~Node()
        {
            Detach();
        }

        Node(Node && b)
        {
            _pList = b._pList;
            _it = b._it;
            b._pList = NULL;
        }

        Node& operator=(Node && b)
        {
            _pList = b._pList;
            _it = b._it;
            b._pList = NULL;
            return *this;
        }

        void Detach()
        {
            if (_pList)
            {
                _pList->erase(_it);
                _pList = NULL;
            }
        }

        operator bool() const
        {
            return (_pList != NULL);
        }

        operator _T_interListIter() const
        {
            return _it;
        }

        _T_List *PList() const
        {
            return _pList;
        }

        void *PObj() const
        {
            if (_pList)
                return _pList->_o;
            return NULL;
        }

        _T_interListIter iter() const
        {
            return _it;
        }

        inline bool IsListType(int type) const
        {
            if (!_pList)
                return false;
            return _pList->ListType == type;
        }

    protected:
        _T_List *_pList;
        _T_interListIter _it;
    };

public:
    using _T_Base::begin;
    using _T_Base::end;
    using _T_Base::rbegin;
    using _T_Base::rend;
    using _T_Base::size;
    using _T_Base::front;
    using _T_Base::back;
    using _T_Base::erase;
    using typename _T_Base::iterator;
    using typename _T_Base::reverse_iterator;
    using _T_Base::empty;


    RefList(void *O, int LType = 0) : _o(O), ListType(LType), _refNodeCallBack(NULL) {};
    RefList(void *O, _T_RefNodeCallBack RefNodeCallBack, int LType = 0) : _o(O), ListType(LType), _refNodeCallBack(RefNodeCallBack){};
    ~RefList()
    {
        clear();
    }

    Node push_back(T c)
    {
        return Node(this, _T_Base::insert(end(), c));
    }

    Node push_front(T c)
    {
        return Node(this, _T_Base::insert(begin(), c));
    }

    Node insert(_T_interListIter iter, T c)
    {
        return Node(this, _T_Base::insert(iter, c));
    }

    void unsafe_clear()
    {
        _T_Base::clear();
    }

    void clear()
    {
        // If here method for return correspondent ref node
        if (_refNodeCallBack)
        {
            while(!empty())
            {
                _refNodeCallBack(front())._pList = NULL;
                erase(begin());
            }
        }
        else
            _T_Base::clear();
    }

    const SafeCopy safe_iter() const
    {
        SafeCopy tmp;
        tmp.reserve(_T_Base::size());
        tmp.assign(_T_Base::begin(), _T_Base::end());
        return tmp;
    }

public:
    void * const _o; // Pointer to object that contain this list
    const int ListType; // Identify list by this

protected:
    _T_RefNodeCallBack _refNodeCallBack; // Used to return correct kidref for this list
};

#endif // LISTNODE_H_INCLUDED
