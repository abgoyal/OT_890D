

#ifndef ContainerNode_h
#define ContainerNode_h

#include "Node.h"
#include "FloatPoint.h"

namespace WebCore {
    
typedef void (*NodeCallback)(Node*);

namespace Private { 
    template<class GenericNode, class GenericNodeContainer>
    void addChildNodesToDeletionQueue(GenericNode*& head, GenericNode*& tail, GenericNodeContainer* container);
};

class ContainerNode : public Node {
public:
    ContainerNode(Document*, bool isElement = false);
    virtual ~ContainerNode();

    Node* firstChild() const { return m_firstChild; }
    Node* lastChild() const { return m_lastChild; }

    virtual bool insertBefore(PassRefPtr<Node> newChild, Node* refChild, ExceptionCode&, bool shouldLazyAttach = false);
    virtual bool replaceChild(PassRefPtr<Node> newChild, Node* oldChild, ExceptionCode&, bool shouldLazyAttach = false);
    virtual bool removeChild(Node* child, ExceptionCode&);
    virtual bool appendChild(PassRefPtr<Node> newChild, ExceptionCode&, bool shouldLazyAttach = false);

    virtual ContainerNode* addChild(PassRefPtr<Node>);
    bool hasChildNodes() const { return m_firstChild; }
    virtual void attach();
    virtual void detach();
    virtual void willRemove();
    virtual IntRect getRect() const;
    virtual void setFocus(bool = true);
    virtual void setActive(bool active = true, bool pause = false);
    virtual void setHovered(bool = true);
    unsigned childNodeCount() const;
    Node* childNode(unsigned index) const;

    virtual void insertedIntoDocument();
    virtual void removedFromDocument();
    virtual void insertedIntoTree(bool deep);
    virtual void removedFromTree(bool deep);
    virtual void childrenChanged(bool createdByParser = false, Node* beforeChange = 0, Node* afterChange = 0, int childCountDelta = 0);

    virtual bool removeChildren();

    void removeAllChildren();

    void cloneChildNodes(ContainerNode* clone);

protected:
    static void queuePostAttachCallback(NodeCallback, Node*);
    void suspendPostAttachCallbacks();
    void resumePostAttachCallbacks();

    template<class GenericNode, class GenericNodeContainer>
    friend void appendChildToContainer(GenericNode* child, GenericNodeContainer* container);

    template<class GenericNode, class GenericNodeContainer>
    friend void Private::addChildNodesToDeletionQueue(GenericNode*& head, GenericNode*& tail, GenericNodeContainer* container);

    void setFirstChild(Node* child) { m_firstChild = child; }
    void setLastChild(Node* child) { m_lastChild = child; }
    
private:
    static void dispatchPostAttachCallbacks();
    
    bool getUpperLeftCorner(FloatPoint&) const;
    bool getLowerRightCorner(FloatPoint&) const;

    Node* m_firstChild;
    Node* m_lastChild;
};

inline ContainerNode::ContainerNode(Document* document, bool isElement)
    : Node(document, isElement, true)
    , m_firstChild(0)
    , m_lastChild(0)
{
}

inline unsigned Node::containerChildNodeCount() const
{
    ASSERT(isContainerNode());
    return static_cast<const ContainerNode*>(this)->childNodeCount();
}

inline Node* Node::containerChildNode(unsigned index) const
{
    ASSERT(isContainerNode());
    return static_cast<const ContainerNode*>(this)->childNode(index);
}

inline Node* Node::containerFirstChild() const
{
    ASSERT(isContainerNode());
    return static_cast<const ContainerNode*>(this)->firstChild();
}

inline Node* Node::containerLastChild() const
{
    ASSERT(isContainerNode());
    return static_cast<const ContainerNode*>(this)->lastChild();
}

} // namespace WebCore

#endif // ContainerNode_h
