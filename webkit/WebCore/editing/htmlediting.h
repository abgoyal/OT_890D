

#ifndef htmlediting_h
#define htmlediting_h

#include <wtf/Forward.h>
#include "HTMLNames.h"
#include "ExceptionCode.h"

namespace WebCore {

class Document;
class Element;
class HTMLElement;
class Node;
class Position;
class Range;
class String;
class VisiblePosition;
class VisibleSelection;

Position rangeCompliantEquivalent(const Position&);
Position rangeCompliantEquivalent(const VisiblePosition&);
int lastOffsetForEditing(const Node*);
bool isAtomicNode(const Node*);
bool editingIgnoresContent(const Node*);
bool canHaveChildrenForEditing(const Node*);
Node* highestEditableRoot(const Position&);
VisiblePosition firstEditablePositionAfterPositionInRoot(const Position&, Node*);
VisiblePosition lastEditablePositionBeforePositionInRoot(const Position&, Node*);
int comparePositions(const Position&, const Position&);
int comparePositions(const VisiblePosition&, const VisiblePosition&);
Node* lowestEditableAncestor(Node*);
Position nextCandidate(const Position&);
Position nextVisuallyDistinctCandidate(const Position&);
Position previousCandidate(const Position&);
Position previousVisuallyDistinctCandidate(const Position&);
bool isEditablePosition(const Position&);
bool isRichlyEditablePosition(const Position&);
Element* editableRootForPosition(const Position&);
Element* unsplittableElementForPosition(const Position&);
bool isBlock(const Node*);
Node* enclosingBlock(Node*);

String stringWithRebalancedWhitespace(const String&, bool, bool);
const String& nonBreakingSpaceString();

//------------------------------------------------------------------------------------------

Position positionBeforeNode(const Node*);
Position positionAfterNode(const Node*);
VisiblePosition visiblePositionBeforeNode(Node*);
VisiblePosition visiblePositionAfterNode(Node*);
PassRefPtr<Range> createRange(PassRefPtr<Document>, const VisiblePosition& start, const VisiblePosition& end, ExceptionCode&);
PassRefPtr<Range> extendRangeToWrappingNodes(PassRefPtr<Range> rangeToExtend, const Range* maximumRange, const Node* rootNode);

PassRefPtr<Range> avoidIntersectionWithNode(const Range*, Node*);
VisibleSelection avoidIntersectionWithNode(const VisibleSelection&, Node*);

bool isSpecialElement(const Node*);
bool validBlockTag(const String&);

PassRefPtr<HTMLElement> createDefaultParagraphElement(Document*);
PassRefPtr<HTMLElement> createBreakElement(Document*);
PassRefPtr<HTMLElement> createOrderedListElement(Document*);
PassRefPtr<HTMLElement> createUnorderedListElement(Document*);
PassRefPtr<HTMLElement> createListItemElement(Document*);
PassRefPtr<HTMLElement> createHTMLElement(Document*, const QualifiedName&);
PassRefPtr<HTMLElement> createHTMLElement(Document*, const AtomicString&);

bool isTabSpanNode(const Node*);
bool isTabSpanTextNode(const Node*);
Node* tabSpanNode(const Node*);
Position positionBeforeTabSpan(const Position&);
PassRefPtr<Element> createTabSpanElement(Document*);
PassRefPtr<Element> createTabSpanElement(Document*, PassRefPtr<Node> tabTextNode);
PassRefPtr<Element> createTabSpanElement(Document*, const String& tabText);

bool isNodeRendered(const Node*);
bool isMailBlockquote(const Node*);
Node* nearestMailBlockquote(const Node*);
unsigned numEnclosingMailBlockquotes(const Position&);
int caretMinOffset(const Node*);
int caretMaxOffset(const Node*);

//------------------------------------------------------------------------------------------

bool isTableStructureNode(const Node*);
PassRefPtr<Element> createBlockPlaceholderElement(Document*);

bool isFirstVisiblePositionInSpecialElement(const Position&);
Position positionBeforeContainingSpecialElement(const Position&, Node** containingSpecialElement=0);
bool isLastVisiblePositionInSpecialElement(const Position&);
Position positionAfterContainingSpecialElement(const Position&, Node** containingSpecialElement=0);
Position positionOutsideContainingSpecialElement(const Position&, Node** containingSpecialElement=0);
Node* isLastPositionBeforeTable(const VisiblePosition&);
Node* isFirstPositionAfterTable(const VisiblePosition&);

Node* enclosingNodeWithTag(const Position&, const QualifiedName&);
Node* enclosingNodeOfType(const Position&, bool (*nodeIsOfType)(const Node*), bool onlyReturnEditableNodes = true);
Node* highestEnclosingNodeOfType(const Position&, bool (*nodeIsOfType)(const Node*));
Node* enclosingTableCell(const Position&);
Node* enclosingEmptyListItem(const VisiblePosition&);
Node* enclosingAnchorElement(const Position&);
bool isListElement(Node*);
HTMLElement* enclosingList(Node*);
HTMLElement* outermostEnclosingList(Node*);
HTMLElement* enclosingListChild(Node*);
bool canMergeLists(Element* firstList, Element* secondList);
Node* highestAncestor(Node*);
bool isTableElement(Node*);
bool isTableCell(const Node*);

bool lineBreakExistsAtPosition(const Position&);
bool lineBreakExistsAtVisiblePosition(const VisiblePosition&);

VisibleSelection selectionForParagraphIteration(const VisibleSelection&);

int indexForVisiblePosition(const VisiblePosition&);
bool isVisiblyAdjacent(const Position& first, const Position& second);
bool isNodeVisiblyContainedWithin(Node*, const Range*);
}

#endif