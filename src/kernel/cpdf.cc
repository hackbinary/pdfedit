// vim:tabstop=4:shiftwidth=4:noexpandtab:textwidth=80
/*
 * $RCSfile$
 *
 * $Log$
 * Revision 1.28  2006/04/17 20:11:47  hockm0bm
 * * OpenMode reorganized
 *         - ReadOnly is first now
 *         - values are ordered according what everything can be done with file
 * * getInstance corrected
 *         - file is opened in append mode (r+) anytime mode is >=ReadWrite
 *           (also for advanced - this didn't work in previous version)
 *
 * Revision 1.27  2006/04/15 08:01:32  hockm0bm
 * * pdfFile field removed
 *         - we are using transparent StreamWriter now
 * * PageTreeWatchDog class implemented as Observer handling page tree changes.
 * * changeIndirectProperty method added
 *         - registers changed property to the XRefWriter
 *
 * Revision 1.26  2006/04/13 18:16:30  hockm0bm
 * insert/removePage, addIndirectProperty, save throws ReadOnlyDocumentException
 *
 * Revision 1.25  2006/04/12 17:55:21  hockm0bm
 * * save method changed
 * 		- fileName parameter removed
 * 		- newRevision parameter added
 * * new method clone
 * * getPage method tested (everything based on this method
 *   should be correct too)
 * * getNodePosition handle ambigues problem
 * 		- not soved yet - new exception should be created
 * * all methods using getNodePosition also handles
 *   problem with ambigues position of page dictionary
 * * registerPageTreeObserver new method
 * * constructor's BaseStream parameter changed to StreamWriter
 *   due to XRefWriter constructor change
 *
 * Revision 1.24  2006/04/01 20:31:15  hockm0bm
 * findPageDict
 *         - Kids array handling errors corrected
 *         - comments updated
 * testing start
 *
 * Revision 1.23  2006/04/01 17:47:29  hockm0bm
 * BUG fixed in findPageDict:
 * 189: shared_ptr<CDict> pages_ptr=IProperty::getSmartCObjectPtr<CDict>(pagesDict);
 *      int count=getIntFromDict("Count", pages_ptr);
 * Problem:
 *      pagesDict is parameter and may be dictionary or reference (in this case
 *      reference). Casting didn't fail so it was used!!!
 * Solution:
 *      dict_ptr is dereferenced dictionary for node.
 *
 * Revision 1.22  2006/04/01 16:45:17  hockm0bm
 * getInstance throws PdfOpenException when file open fails
 *
 * Revision 1.21  2006/03/31 22:36:52  hockm0bm
 * names changed from /Name -> Name
 *
 * Revision 1.20  2006/03/31 21:03:00  hockm0bm
 * removePage implemented
 *
 * Revision 1.19  2006/03/30 23:21:14  misuj1am
 *
 *
 *  -- getPropertyValue -> getProperty
 *
 * Revision 1.18  2006/03/30 21:29:33  hockm0bm
 * * findPage renamed to findPageDict
 *         - return value changed to shared_ptr<CDict>
 * * TODOs for exceptions unification
 * * code simplification for dictionary handling
 *         - using utils::getDictFromRef more often
 * * printDbg for more methods
 * * insertPage implemented
 *
 * Revision 1.17  2006/03/29 06:12:34  hockm0bm
 * consolidatePageTree method added
 * starting to use getPageFromRef
 *
 * Revision 1.16  2006/03/27 22:28:42  hockm0bm
 * consolidatePageList method added to CPdf
 *
 * Utils namespace:
 * isDescendant method for subtree information
 * getNodePosition method for CDict -> position
 * searchTreeNode helper method
 *
 *
 */

#include <errno.h>
#include "static.h"

#include "iproperty.h"
#include "cobject.h"
#include "cobjecthelpers.h"
#include "cpdf.h"
#include "factories.h"
#include "utils/debug.h"

// =====================================================================================

using namespace boost;
using namespace std;
using namespace debug;


// TODO exceptions unification

namespace pdfobjects
{

typedef std::vector<boost::shared_ptr<IProperty> > ChildrenStorage;

namespace utils 
{
	
/** Helper method to find page at certain position.
 * @param CPdf Pdf instance where to search.
 * @param pagesDict Reference to or Page or Pages dictionary representing 
 * page node (see Pdf standard notes).
 * @param startPos Starting position for searching (see note below).
 * @param pos Page position (starting from 1) to find.
 *
 * Method recursively goes through pages dictionary until given page 
 * position is found or no such position can be found. If position can't be
 * found under given page node, exception is thrown.
 * <br>
 * If given pagesDict is reference, uses CPdf::getIndirectProperty to get 
 * dictionary.
 * 
 * <p>
 * <b>Pdf standard notes</b>:
 * <br>
 * Pdf tree structure contains of two types of page dictionaries. 
 * <ul>
 * <li>Pages dictionary - which is just intermediate node in the tree and
 * contains children nodes. These may be direct pages or another Pages node.
 * It doesn't represent page itself. All children are stored in Kids array.
 * Dictionary also contains Count information which holds number of all
 * Page dictionaries under this node.
 * <li>Page dictionary - which represents direct page.
 * </ul>
 * This structure is rather complex but enables effective way to access 
 * arbitrary page in short time (some applications provide balanced tree form
 * to enable very effective access).
 * <p>
 * <b>Implementation notes</b>:<br>
 * When starting to search from root of all pages, pagesDict should be supplied
 * from pdf->getProperty("Pages"). This is indirect reference (according
 * standard), but can be used in this method. startPos should be 1 (first page
 * is 1).
 * <br>
 * If we have some intermediate <b>Pages</b> dictionary, startPos should be
 * lowest page number under this tree branch. This usage is recomended only if
 * caller exactly knows what he is doing, otherwise page position is wrong.
 *
 * @throw PageNotFoundException if given position couldn't be found.
 * @throw ElementBadTypeException if some of required element has bad type.
 * @throw MalformedFormatExeption if page node count number doesn't match the
 * reality (page count number is not reliable).
 * @return Dereferenced page (wrapped in shared_ptr) dictionary at given 
 * position.
 */
boost::shared_ptr<CDict> findPageDict(CPdf & pdf, boost::shared_ptr<IProperty> pagesDict, size_t startPos, size_t pos)
{
	// TODO error handling unification
	
	printDbg(DBG_DBG, "startPos=" << startPos << " pos=" << pos);
	if(startPos > pos)
	{
		printDbg(DBG_ERR, "startPos > pos");
		// impossible to find such page
		throw PageNotFoundException(pos);
	}

	// dictionary smart pointer holder
	// it is initialized according pagesDict parameter - if it is reference
	// it has to be dereferenced
	shared_ptr<CDict> dict_ptr;
	PropertyType type=pagesDict->getType();

	// checks if given parameter is reference and if so, dereference it
	// using getIndirectProperty method and casts to dict_ptr
	// otherwise test if given type is dictionary and casts to dict_ptr
	if(type==pRef)
	{
		printDbg(DBG_DBG, "pagesDict is reference");
		try
		{
			dict_ptr=getDictFromRef(pagesDict);
		}catch(ElementBadTypeException & e)
		{
			// malformed pdf
			throw MalformedFormatExeption("Reference target is not dictionary");
		}
	}else
	{
		if(type!=pDict)
		{
			printDbg(DBG_ERR, "pagesDict is not dictionary type="<< type);
			// maloformed pdf
			throw ElementBadTypeException("pagesDict");
		}
		dict_ptr=IProperty::getSmartCObjectPtr<CDict>(pagesDict);
	}

	// gets type property from dictionary to find out what to do
	string dict_type=getNameFromDict("Type", dict_ptr);
	printDbg(DBG_DBG, "dictionary type=" << dict_type);
	
	// if type is Page then we have page dictionary and so start_pos and pos 
	// must match otherwise it is not possible to find pos page
	if(dict_type=="Page")
	{
		printDbg(DBG_DBG, "Page node is direct page");
		// everything ok 
		if(startPos == pos)
		{
			printDbg(DBG_INFO, "Page found");
			return dict_ptr;
		}
		
		// unable to find
		printDbg(DBG_ERR, "Page not found startPos="<<startPos);
		throw PageNotFoundException(pos);
	}

	// if type is Pages then we are in intermediate node
	// in this case we have to start to dereference children from Kids array.
	if(dict_type=="Pages")
	{
		printDbg(DBG_DBG, "Page node is intermediate");
		int count=getIntFromDict("Count", dict_ptr);

		printDbg(DBG_DBG, "Node has " << count << " pages");
		// check if this subtree contains enought direct pages 
		if(count + startPos <= pos )
		{
			printDbg(DBG_ERR, "page can't be found under this subtree startPos=" << startPos);
			// no way to find given position under this subtree
			throw PageNotFoundException(pos);
		}

		// PAGE IS IN THIS SUBTREE, we have to find where
		
		// gets Kids array from pages dictionary and gets all its children
		shared_ptr<IProperty> kidsProp_ptr=dict_ptr->getProperty("Kids");
		if(kidsProp_ptr->getType()!=pArray)
		{
			printDbg(DBG_ERR,"Kids field is not an array. type="<<kidsProp_ptr->getType());
			throw ElementBadTypeException("Kids");
		}
		shared_ptr<CArray> kids_ptr=IProperty::getSmartCObjectPtr<CArray>(kidsProp_ptr);
		ChildrenStorage children;
		kids_ptr->_getAllChildObjects(children);

		// min_pos holds minimum position for actual child (at the begining
		// startPos value and incremented by page number in node which can't
		// contain pos (normal page 1 and Pages their count).
		ChildrenStorage::iterator i;
		size_t min_pos=startPos, index=0;
		for(i=children.begin(); i!=children.end(); i++, index++)
		{
			shared_ptr<IProperty> child=*i;

			PropertyType childType=child->getType();
			printDbg(DBG_DBG, index << " kid checking if it is reference");
			// all members of Kids array have to be referencies according
			// PDF specification
			if((*i)->getType() != pRef)
			{
				// malformed pdf
				printDbg(DBG_ERR, "Pages Kid must be reference");
				throw ElementBadTypeException(""+index);
			}

			// we have to dereference and indirect object has to be dictionary
			// child_ptr finally contains this dictionary
			shared_ptr<CDict> child_ptr; 
			try
			{
				child_ptr=getDictFromRef(*i);
			}catch(ElementBadTypeException & e)
			{
				// target is not an dictionary
				throw MalformedFormatExeption("Kid target is not dictionary");
			}
			
			// we can have page or pages dictionary in child_ptr
			// Type field determine kind of node
			string dict_type=getNameFromDict("Type", child_ptr);
			printDbg(DBG_DBG, "kid type="<<dict_type);

			// Page dictionary is ok if min_pos equals pos, 
			// otherwise it is skipped - can't use recursion here because it's
			// not an error that this page is not correct one
			// min_pos is incremented
			if(dict_type=="Page")
			{
				if(min_pos == pos)
				{
					printDbg(DBG_INFO, "page found");
					return child_ptr;
				}
				min_pos++;
				continue;
			}

			// Pages dictionary is checked for its page count and if pos can
			// be found there, starts recursion
			// Otherwise increment min_pos with its count and continues
			if(dict_type=="Pages")
			{
				int count=getIntFromDict("Count", child_ptr);

				if(min_pos + count > pos )
					// pos IS in this subtree
					return findPageDict(pdf, child_ptr, min_pos, pos);

				// pos is not in this subtree
				// updates min_pos with its count and continues
				min_pos+=count;

				continue;
			}

			// malformed pdf
			printDbg(DBG_ERR, "kid dictionary doesn't have correct Type field");
			throw ElementBadTypeException(""+index);
		}

		// this should never happen, because given pos was in this subtree
		// according Count Pages field
		// PDF malformed 
		printDbg(DBG_ERR, "pages count field is not correct");
		throw MalformedFormatExeption("Page node count number is not correct");
	}
	
	// should not happen - malformed pdf document
	printDbg(DBG_ERR, "pagesDict dictionary doesn't have correct Type field");
	throw ElementBadTypeException("pagesDict");

}

/** Searches node in page tree structure.
 * @param pdf Pdf where to search.
 * @param superNode Node which to search.
 * @param node Node to be found.
 * @param startValue Position for first found node in this superNode.
 *
 * At first checks if node and superNode are same nodes (uses == operator to
 * compare). Then tries to get Type of the dictionary. In Page case returns with
 * startValue if given nodes are same or 0 (page not found).
 * If type is Pages (intermediate node) goes through Kids array and recursively
 * calls this method for each element until recursion returns with non 0 result.
 * This means the end of recursion. startValue is actualized for each Kid's
 * element (Page element increases 1, Pages element Count).
 * <br>
 * If node is found as direct Kids member (this means that reference of target 
 * node is direct memeber of Kids array), then determines if the node position 
 * is unambigues - checks whether reference to the node is unique in Kids array. 
 * If not (TODO which) exception is thrown. This means that searchTreeNode 
 * function is not able to definitively determine node's position.
 *
 * @throw ElementBadTypeException If required element of pdf object has bad
 * type.
 * @throw ElementNotFoundException If required element of pdf object is not
 * found.
 *
 * @return Position of the node or 0 if node couldn't be found under this
 * superNode.
 */
size_t searchTreeNode(CPdf & pdf, shared_ptr<CDict> superNode, shared_ptr<CDict> node, size_t startValue)
{
	// TODO error handling unification
	
	printDbg(DBG_DBG, "startPos="<<startValue);
	
	// if nodes are same, startValue is returned.
	if(superNode==node)
	{
		printDbg(DBG_DBG, "Page found");
		return startValue;
	}

	// gets type of dictionary
	string superNodeType=getNameFromDict("Type", superNode);
	
	// if type is Page we return with 0, because node is not this superNode and
	// there is nowhere to go
	if(superNodeType=="Page")
		return 0;

	// if node is not also Pages - page tree is not well formated and we skip
	// this node
	if(superNodeType!="Pages")
	{
		printDbg(DBG_WARN, "Geven dictionary is not correct page tree node. type="<<superNodeType);
		return 0;
	}

	// if type is Pages, we go through Kids array and calls recursively for each
	// element until first one returns with non 0. Otherwise startValue is
	// updated by child node pages count.
	shared_ptr<IProperty> arrayProp_ptr=superNode->getProperty("Kids");
	if(arrayProp_ptr->getType()!=pArray)
	{
		printDbg(DBG_ERR, "Supernode Kids field is not an array. type="<<arrayProp_ptr->getType());
		throw ElementBadTypeException("Kids");
	}
	shared_ptr<CArray> array_ptr=IProperty::getSmartCObjectPtr<CArray>(arrayProp_ptr);

	// Checks if node's reference is presnt in Kids array
	// All nodes has to be indirect objects so getIndiRef returns node's
	// reference
	// If getPropertyId returns more than one element - position is ambigues and
	// exception is thrown 
	CRef nodeRef(node->getIndiRef());
	// TODO array_ptr->getPropertyId();
	
	// node is not direct child of superNode we have to go deaper in subtree
	vector<shared_ptr<IProperty> > kidsContainer;
	array_ptr->_getAllChildObjects(kidsContainer);
	vector<shared_ptr<IProperty> >::iterator i;
	int index=0;
	for(i=kidsContainer.begin(); i!=kidsContainer.end(); i++, index++)
	{
		// each element has to be reference
		PropertyType type=(*i)->getType();
		if(type!=pRef)
		{
			printDbg(DBG_ERR, "Kids["<<index<<"] is not an reference. type="<<type);
			throw ElementBadTypeException("Kids");
		}

		// dereference indirect object - this has to be indirect object
		IndiRef ref;
		IProperty::getSmartCObjectPtr<CRef>(*i)->getPropertyValue(ref);
		shared_ptr<IProperty> element_ptr=pdf.getIndirectProperty(ref);
		if(element_ptr->getType()!=pDict)
		{
			printDbg(DBG_ERR, "Kids["<<index<<"] doesn't refer to a dictionary. type="<<element_ptr->getType());
			throw ElementBadTypeException("");
		}
		shared_ptr<CDict> elementDict_ptr=IProperty::getSmartCObjectPtr<CDict>(element_ptr);

		// starts recursion on elementDict_ptr as new superNode for its subtree
		// checks Type field of elementDict_ptr dictionary to know how to update
		// startValue if not found in this subtree
		string elementDictType=getNameFromDict("Type", elementDict_ptr);
		if(elementDictType=="Page")
		{
			// this was not correct one because reference was not found in Kids, 
			// increments startValue
			startValue++;
			continue;
		}
		if(elementDictType=="Pages")
		{
			size_t ret=searchTreeNode(pdf, elementDict_ptr, node, startValue);
			if(ret)
				return ret;

			// we haven't found, so startValue is actualized with Count value
			// of this node
			startValue+=getIntFromDict("Count", elementDict_ptr);
			continue;
		}
		
		// if node is not Page or Pages it is ignored.
		printDbg(DBG_WARN, "Kids element dictionary is not Page or Pages node.");
	}

	return 0;
}

/** Gets position of given node.
 * @param pdf Pdf where to examine.
 * @param node Node to find (CRef or CDict instances).
 *
 * Start searching of given node from root of the page tree (document catalog
 * Pages field). Uses recursive searchTreeNode function for searching and
 * provides just error handling wrapper to this method.
 * <br>
 * Prefer to use this function instead of searchTreeNode if you are not sure you
 * know what you are doing.
 *
 * @throw PageNotFoundException
 * @throw ElementBadTypeException
 * @throw MalformedFormatExeption
 * @return Node position.
 */
size_t getNodePosition(CPdf & pdf, shared_ptr<IProperty> node)
{
	// TODO error handling unification

	printDbg(DBG_DBG, "");
	// node must by from given pdf
	if(node->getPdf()!=&pdf)
	{
		printDbg(DBG_ERR, "Node is not from given pdf isntance.");
		throw PageNotFoundException(0);
	}

	// gets page tree root - it has to be Reference to Dictionary
	shared_ptr<IProperty> rootRef_ptr=pdf.getDictionary()->getProperty("Pages");
	shared_ptr<CDict> rootDict_ptr=getDictFromRef(rootRef_ptr);
	
	// gets dictionary from node parameter. It can be reference and
	// dereferencing has to be done or direct dictionary - otherwise error is
	// reported
	PropertyType nodeType=node->getType();
	if(nodeType!=pRef && nodeType!=pDict)
	{
		printDbg(DBG_ERR, "Given node is not reference nor dictionary. type="<<nodeType);
		throw ElementBadTypeException("node");
	}
	shared_ptr<CDict> nodeDict_ptr;
	if(nodeType==pRef)
		nodeDict_ptr=getDictFromRef(node);
	else
		nodeDict_ptr=IProperty::getSmartCObjectPtr<CDict>(node);
		
	printDbg(DBG_DBG, "Starts searching");
	size_t pos=searchTreeNode(pdf, rootDict_ptr, nodeDict_ptr, 1);
	printDbg(DBG_DBG, "Searching finished. pos="<<pos);
	if(pos)
		return pos;

	// node not found
	throw PageNotFoundException(0);
}

/** Checks if given child is descendant of node with given reference.
 * @param pdf Pdf where to resolv referencies.
 * @param parent Reference of the parent.
 * @param child Dictionary of page(s) node.
 *
 * Checks if child's Parent field has same reference as given one as parent
 * parameter. If yes then child dictionary is descendant of node with parent
 * reference. If not, dereference child's parent and continues in recursion
 * using derefenced direct parent as new child for recursion call.
 * <br>
 * NOTE: this method doesn't perform any checking of parameters.
 *
 * @throw MalformedFormatExeption if child contains Parent field which doesn't
 * point to the dictionary.
 * @return true If given child belongs to parent subtree, false otherwise.
 */
bool isDescendant(CPdf & pdf, IndiRef parent, shared_ptr<CDict> child)
{
using namespace utils;

	// tries to get child's direct parent node
	// if not found, we catch the exception and return false
	// this should happen in root node
	IndiRef directParent;
	try
	{
		directParent=getRefFromDict("Parent", child);
	}catch(std::exception & e)
	{
		// child is not really parent child
		return false;
	}

	// we have direct parent reference
	// if it is same as given parent, we are done and return true
	if(parent==directParent)
		return true;

	// TODO solve problem when parent is the root of hierarchy and its Parent
	// field points to itself
	
	// parent may be somewhere higher in the page tree
	// Gets direct parent indirect object from given pdf and checks its type
	// if it is NOT dictionary - we have malformed pdf - exception is thrown
	// otherwise starts recursion with direct parent dictionary
	shared_ptr<IProperty> directParent_ptr=pdf.getIndirectProperty(directParent);
	if(directParent_ptr->getType()!=pDict)
		throw MalformedFormatExeption("Page node parent field doesn't point to dictionary");
	return isDescendant(pdf, parent, IProperty::getSmartCObjectPtr<CDict>(directParent_ptr));
}

} // end of utils namespace



/** Registers page tree observer.
 * @param ref Reference to page tree node.
 * @param observer Pointer to observer to register.
 *
 * Registers given observer to given reference. Then gets indirect property from
 * reference and if it is Pages dictionary, Registers observer also on Kids
 * array and recursively to all its elements (which are referencies).
 */
void registerPageTreeObserver(boost::shared_ptr<CRef> ref, const observer::IObserver<IProperty> * observer)
{
using namespace boost;
using namespace std;
using namespace pdfobjects::utils;

	IndiRef indiRef;
	ref->getPropertyValue(indiRef);
	printDbg(DBG_DBG, "ref=["<<indiRef.num<<", "<<indiRef.gen<<"]");

	// registers observer for page tree handling
	// FIXME uncomment
	//ref->registerObserver(observer);
	
	// dereferences and if it is Pages dictionary, calls recursively to all
	// children
	shared_ptr<CDict> dict_ptr=getDictFromRef(ref);
	string dictType=getNameFromDict("Type", dict_ptr);

	// if this is not pages dictionary, immediately returns
	if(dictType!="Pages")
		return;

	printDbg(DBG_DBG, "reference to Pages dictionary.");

	// gets Kids field from dictionary and all children from array and 
	// registers observer to array and to each member reference
	shared_ptr<IProperty> kidsProp_ptr=dict_ptr->getProperty("Kids");
	if(kidsProp_ptr->getType()!=pArray)
	{
		// Kids is not an array - this is not an hard error at this context
		printDbg(DBG_WARN, "Kids field is not an array. type="<<kidsProp_ptr->getType());
		return;
	}
	shared_ptr<CArray> kids_ptr=IProperty::getSmartCObjectPtr<CArray>(kidsProp_ptr);
	// FIXME uncomment
	//kids_ptr->registerObserver(observer);
	vector<shared_ptr<IProperty> > container;
	kids_ptr->_getAllChildObjects(container);
	for(vector<shared_ptr<IProperty> >::iterator i=container.begin(); i!=container.end(); i++)
	{
		// all reference kids are used for recursion call
		shared_ptr<IProperty> elemProp_ptr=*i;
		if(elemProp_ptr->getType()==pRef)
			registerPageTreeObserver(IProperty::getSmartCObjectPtr<CRef>(elemProp_ptr), observer);
	}

	printDbg(DBG_DBG, "All subnodes done for ref=["<<indiRef.num<<", "<<indiRef.gen<<"]");
}

void CPdf::PageTreeWatchDog::notify(boost::shared_ptr<IProperty> newValue, boost::shared_ptr<const observer::IChangeContext<IProperty> > context) const throw()
{
	using namespace observer;
	using namespace boost;
	using namespace debug;

	// if not basic context type, does notnig
	if(context->getType()!=IChangeContext<IProperty>::BasicChangeContextType)
		return;

	// gets basic context shared_ptr
	shared_ptr<const BasicChangeContext<IProperty> > basicContext=
		dynamic_pointer_cast<const BasicChangeContext<IProperty>, const IChangeContext<IProperty> >(context); 

	if(newValue->getType()!=pNull)
	{
		// if newValue is not CNull and it is CRef instance, registers this 
		// observer on it and if it is intermediate node, also to all children.
		// Uses registerPageTreeObserver method
		if(newValue->getType()==pRef)
		{
			shared_ptr<CRef> newValueRef_ptr=IProperty::getSmartCObjectPtr<CRef>(newValue);
			try
			{
				registerPageTreeObserver(newValueRef_ptr, this);
			}catch(CObjectException & e)
			{
				printDbg(DBG_ERR, "registerPageTreeObserver failed with cause="<<e.what());
			}
		}

		// consolidates page tree starting from parent of newValue
		// node - given value is always direct value
		// Doesn't care if it is reference or not, because parent node has to be
		// consolidated in any case (may be there is nothing to do, but we can't
		// rely on it)
		IndiRef parentRef=newValue->getIndiRef();
		shared_ptr<IProperty> parentProp_ptr=pdf->getIndirectProperty(parentRef);
		if(parentProp_ptr->getType()!=pDict)
		{
			// target of the parent reference is not dictionary,
			// this should not happen - some one is doing something nasty
			printDbg(DBG_WARN, "newValue's parent is not dictionary. THIS SHOUL NOT HAPPEN");
		}
		else
		{
			shared_ptr<CDict> parentDict_ptr=IProperty::getSmartCObjectPtr<CDict>(parentProp_ptr);
			try
			{
				pdf->consolidatePageTree(parentDict_ptr);
			}catch(CObjectException & e)
			{
				printDbg(DBG_ERR, "consolidatePageTree failed with cause="<<e.what());
			}
		}
	}

	// consolidates pageList 
	try
	{
		pdf->consolidatePageList(basicContext->getOriginalValue(), newValue);
	}catch(CObjectException &e)
	{
		printDbg(DBG_ERR, "consolidatePageList failed with cause="<<e.what());
	}
}

void CPdf::initRevisionSpecific()
{

	printDbg(DBG_DBG, "");

	// Clean up part:
	// =============
	
	// cleans up indirect mapping
	if(indMap.size())
	{
		printDbg(DBG_INFO, "Cleaning up indirect mapping with "<<indMap.size()<<" elements");
		indMap.clear();
	}

	// cleans up and invalidates all returned pages
	if(pageList.size())
	{
		printDbg(DBG_INFO, "Cleaning up pages list with "<<pageList.size()<<" elements");
		PageList::iterator i;
		for(i=pageList.begin(); i!=pageList.end(); i++)
		{
			printDbg(DBG_DBG, "invalidating page at pos="<<i->first);
			// FIXME uncoment when method is reay
			//i->second->invalidate();
		}
		pageList.clear();
	}
	
	// cleanup all returned outlines  -------------||----------------- 
	
	// Initialization part:
	// ===================
	
	// initialize trailer dictionary from xpdf trailer dictionary object
	// no free should be called because trailer is returned directly from XRef
	Object * trailerObj=xref->getTrailerDict();
	printDbg(DBG_DBG, "Creating trailer dictionary from type="<<trailerObj->getType());
	trailer=boost::shared_ptr<CDict>(CDictFactory::getInstance(*trailerObj));
	
	// Intializes document catalog dictionary.
	// gets Root field from trailer, which should contain reference to catalog.
	// If no present or not reference, we have corrupted PDF file and exception
	// is thrown
	printDbg(DBG_DBG, "Getting Root of page tree");
	IndiRef rootRef=utils::getRefFromDict("Root", trailer);
	shared_ptr<IProperty> prop_ptr=getIndirectProperty(rootRef);
	if(prop_ptr->getType()!=pDict)
	{
		printDbg(DBG_CRIT, "Trailer dictionary doesn't point to correct document catalog.");
		throw ElementBadTypeException("Root");
	}
	printDbg(DBG_INFO, "Document catalog successfully fetched");
	docCatalog=IProperty::getSmartCObjectPtr<CDict>(prop_ptr);
	
	// calls registerPageTreeObserver to Document catalog Pages field which
	// means that observer is registered to whole page tree.
	// If Document catalog doesn't contain any Pages field, we don't have any
	// pages and so nothing is registered
	try
	{
		shared_ptr<IProperty> pageTreeProp_ptr=docCatalog->getProperty("Pages");
		if(pageTreeProp_ptr->getType()!=pRef)
		{
			printDbg(DBG_ERR, "Pages field is not reference.");	
			// TODO handle
		}else
		{
			shared_ptr<CRef> pageTreeRoot_ptr=IProperty::getSmartCObjectPtr<CRef>(pageTreeProp_ptr);
			registerPageTreeObserver(pageTreeRoot_ptr, &pageTreeWatchDog);
		}
	}catch(CObjectException & e)
	{
		printDbg(DBG_WARN, "Document doesn't contain Page tree.");
	}
}

CPdf::CPdf(StreamWriter * stream, OpenMode openMode):pageTreeWatchDog(PageTreeWatchDog(this))
{
	// gets xref writer - if error occures, exception is thrown 
	xref=new XRefWriter(stream, this);
	mode=openMode;

	// initializes revision specific data for the newest revision
	initRevisionSpecific();
}

CPdf::~CPdf()
{
	printDbg(DBG_DBG, "");

	// indirect mapping is cleaned up automaticaly
	
	// discards all returned pages
	for(PageList::iterator i=pageList.begin(); i!=pageList.end(); i++)
	{
		printDbg(DBG_DBG, "Invalidating page at pos="<<i->first);
		// FIXME uncoment when method is ready
		//i->second->invalidate();
	}

	// TODO handle outlines when ready
}


//
// 
//
boost::shared_ptr<IProperty> CPdf::getIndirectProperty(IndiRef ref)
{
using namespace debug;

	printDbg (0,"ref=["<<ref.num << "," << ref.gen <<"]");

	// find the key, if it exists
	IndirectMapping::iterator i = indMap.find(ref);
	if(i!=indMap.end())
	{
		// mapping exists, so returns value
		printDbg(DBG_DBG, "mapping exists");
		return i->second;
	}

	printDbg(DBG_DBG, "mapping doesn't exist")

	// mapping doesn't exist yet, so tries to create one
	// fetches object according reference
	Object obj;
	xref->fetch(ref.num, ref.gen, &obj);
	
	// creates cobject from value according type - indirect
	// parent is set to object reference (it is its own indirect parent)
	// created object is wrapped to smart pointer and if not pNull also added to
	// the mapping
	IProperty * prop=utils::createObjFromXpdfObj(*this, obj, ref);
	boost::shared_ptr<IProperty> prop_ptr(prop);
	if(prop_ptr->getType() != pNull)
	{
		indMap.insert(IndirectMapping::value_type(ref, prop_ptr));
		printDbg(DBG_DBG, "Mapping created");
	}

	obj.free ();
	return prop_ptr;
}

/** Adds all indirect objects from given property.
 * @param pdf Pdf file where to put indirect objects.
 * @param ip Property to examine.
 *
 * Ckecks given property for type and if it is reference, uses 
 * CPdf::addIndirectProperty to add indirect object to the given pdf file.
 * Otherwise checks if ip is complex type and if so, checks all children 
 * recursively (calls this method on each). If called method returns with
 * non NULL reference, changes child to contain new reference pointing to
 * newly created object. All other simple values are just ignored.
 * <br>
 * Given ip may change its child value (if contains reference).
 * <br>
 * After this method returns, all subproperties of given one are known to given
 * pdf instance.
 * <br>
 * Returned reference has to be deallocated by caller.
 * 
 * @return Pointer to new reference, if new indirect property was created
 * for reference (caller must deallocate result).
 */ 
IndiRef * addReferencies(CPdf * pdf, boost::shared_ptr<IProperty> ip)
{
	printDbg(debug::DBG_DBG,"");

	PropertyType type=ip->getType();
	ChildrenStorage childrenStorage;

	switch(type)
	{
		case pRef:
		{
			printDbg(debug::DBG_DBG,"Property is reference - adding new indirect object");
			// this may fail because of ReadOnlyDocumentException
			IndiRef indiRef=pdf->addIndirectProperty(ip);
			return new IndiRef(indiRef);
		}	
		// complex types (pArray and pDict) collects their children to the 
		// container
		case pArray:
			IProperty::getSmartCObjectPtr<CArray>(ip)->_getAllChildObjects(childrenStorage);
			break;
		case pDict:
			IProperty::getSmartCObjectPtr<CDict>(ip)->_getAllChildObjects(childrenStorage);
			break;

		// all other simple values are ok, nothing should return
		default:
			return NULL;
	}

	// go throught all collected children and recursively calls this
	// method on each. If return value is not NULL, new object has been
	// created and sets new reference for child
	ChildrenStorage::iterator i;
	for(i=childrenStorage.begin(); i!=childrenStorage.end(); i++)
	{
		IndiRef * ref=addReferencies(pdf, *i);
		if(ref)
		{
			// new reference for this child
			boost::shared_ptr<CRef> ref_ptr=IProperty::getSmartCObjectPtr<CRef>(*i);
			ref_ptr->writeValue(*ref);
			printDbg(debug::DBG_DBG,"Reference changed to [" << ref->num << ", " <<ref->gen << "]");
			delete ref;
			continue;
		}
	}

	// also complex object is same - all referencies in this subtree are added
	// in this moment
	return NULL;
}

//
//
//
IndiRef CPdf::addIndirectProperty(boost::shared_ptr<IProperty> ip)
{
	printDbg(debug::DBG_DBG, "");

	if(mode==ReadOnly)
	{
		printDbg(DBG_ERR, "Document is opened in read-only mode");
		throw ReadOnlyDocumentException("Document is in read-only mode.");
	}
	
	// place for propertyValue
	// it is ip by default
	boost::shared_ptr<IProperty> propValue=ip;
	
	// if we given ip is reference, we have to distinguish whether
	// it comes from same file (and then nothing is to do and we
	// just simply return ip reference) 
	// or from different file, when we need to dereference and after 
	// something else can be done
	PropertyType type=ip->getType();
	if(type==pRef)
	{
		IndiRef ref;
		boost::shared_ptr<CRef> cref_ptr=IProperty::getSmartCObjectPtr<CRef>(ip);
		cref_ptr->getPropertyValue(ref);
		
		// just returns reference
		if(ip->getPdf()==this)
		{
			printDbg(debug::DBG_WARN, "Reference is from this file, nothing is added.");
			return ref;
		}

		// dereference real value
		propValue=ip->getPdf()->getIndirectProperty(ref); 
	}

	// All complex property values from different pdfs (not only referencies 
	// handled above) have to be searched for referencies inside, those are
	// added too
	if(propValue->getPdf() != this)
	{
		printDbg(debug::DBG_DBG, "Adding property from different file.");
		boost::shared_ptr<IProperty> clone;
		switch(propValue->getType())
		{
			// creates local clone for complex types - to keep 
			// original value untouched - and adds all referencies
			// in subtree
			// clone will contain correct referencies after
			// addReferencies function returns so changes value of
			// propValue
			case pDict:
			case pArray:
				clone=propValue->clone();
				addReferencies(this, clone);
				propValue=clone;
				break;
			default:
				// just to be gcc happy and prints no warning for
				// unhandled members from enum
				// we don't have to do anything here because
				// simple values can't contain referencies
				break;
			
		}

		// propValue contans real object that we want to add now
	}


	// gets xpdf Object from propValue (which contain definitive value to
	// be stored), registers new reference on xref
	// reserveRef may throw if we are in older revision
	::Object * obj=propValue->_makeXpdfObject();
	::Ref xpdfRef=xref->reserveRef();
	printDbg(debug::DBG_DBG, "New reference reseved ["<<xpdfRef.num<<", "<<xpdfRef.gen<<"]");
	xref->changeObject(xpdfRef.num, xpdfRef.gen, obj);

	// xpdf object has to be deallocated
	utils::freeXpdfObject(obj);

	// creates return value from xpdf reference structure
	// and returns
	IndiRef reference={xpdfRef.num, xpdfRef.gen};
	printDbg(debug::DBG_INFO, "New indirect object inserted with reference ["<<xpdfRef.num<<", "<<xpdfRef.gen<<"]");
	return reference;
}

void CPdf::changeIndirectProperty(boost::shared_ptr<IProperty> prop)
{
	printDbg(DBG_DBG, "");
	
	// checks property at first
	// it must be from same pdf
	if(prop->getPdf() != this)
	{
		printDbg(DBG_ERR, "Given property is not from same pdf.");
		// TODO exception
	}
	// there must be mapping fro prop's indiref, but it doesn't have to be same
	// instance.
	IndiRef indiRef=prop->getIndiRef();
	if(indMap.find(indiRef)==indMap.end())
	{
		printDbg(DBG_ERR, "Indirect mapping doesn't exist. prop seams to be fake.");
		// TODO exception
	}

	// gets xpdf Object instance and calls xref->change
	// changeObject meay throw if we are in read only mode or if xrefwriter is
	// in paranoid mode and type check fails
	Object * propObject=prop->_makeXpdfObject();
	printDbg(DBG_DBG, "Registering change to the XRefWriter");
	xref->changeObject(indiRef.num, indiRef.gen, propObject);
	// FIXME SEGV problem 
	//utils::freeXpdfObject(propObject);

	// invalidates indMap mapping for this property
	// This is because prop may be something different than original indirect
	// property not just different state of same property. Mapping is created
	// again when it is needed (when getIndirectProperty is called)
	indMap.erase(indiRef);
	printDbg(DBG_INFO, "Indirect mapping removed for ref=["<<indiRef.num<<", "<<indiRef.gen<<"]");
}

CPdf * CPdf::getInstance(const char * filename, OpenMode mode)
{
	printDbg(debug::DBG_DBG, "");
	
	// openMode is read-only by default
	const char * openMode="r";
	
	// if mode is ReadWrite or higher, set to read-write mode starting at the 
	// begining.
	if(mode >= ReadWrite)
		openMode="r+";

	// opens file and creates (xpdf) FileStream
	FILE * file=fopen(filename, openMode);
	if(!file)
	{
		printDbg(debug::DBG_ERR, "Unable to open file (reason="<<strerror(errno)<<")");
		throw PdfOpenException("Unable to open file.");
	}
	printDbg(debug::DBG_DBG,"File \"" << filename << "\" open successfully in mode=" << openMode);
	
	// creates FileStream writer to enable changes to the File stream
	Object obj;
	obj.initNull();
	StreamWriter * stream=new FileStreamWriter(file, 0, gFalse, 0, &obj);
	printDbg(debug::DBG_DBG,"File stream created");

	// stream is ready, creates CPdf instance
	CPdf * instance=new CPdf(stream, mode);

	printDbg(debug::DBG_INFO, "Instance created successfully openMode=" << openMode);

	return instance;
}

int CPdf::close(bool saveFlag)
{
	printDbg(debug::DBG_DBG, "");
	// saves if necessary
	if(saveFlag)
		save();
	
	// deletes this instance
	// all clean-up is made in destructor
	delete this;

	printDbg(debug::DBG_INFO, "Instance deleted.")
	return 0;
}

boost::shared_ptr<CPage> CPdf::getPage(size_t pos)
{
using namespace utils;

	printDbg(DBG_DBG, "");

	if(pos < 1 || pos>getPageCount())
	{
		printDbg(DBG_ERR, "Page out of range pos="<<pos);
		throw PageNotFoundException(pos);
	}

	// checks if page is available in pageList
	PageList::iterator i;
	if((i=pageList.find(pos))!=pageList.end())
	{
		printDbg(DBG_INFO, "Page at pos="<<pos<<" found in pageList");
		return i->second;
	}

	// page is not available in pageList, searching has to be done
	// gets Pages field from document catalog (no special checking has to be
	// done, because everything is done in getPageCount method).
	shared_ptr<IProperty> rootPages_ptr=docCatalog->getProperty("Pages");
	// find throws an exception if any problem found, otherwise pageDict_ptr
	// contians Page dictionary at specified position.
	shared_ptr<CDict> pageDict_ptr=findPageDict(*this, rootPages_ptr, 1, pos);

	// creates CPage instance from page dictionary and stores it to the pageList
	CPage * page=CPageFactory::getInstance(pageDict_ptr);
	shared_ptr<CPage> page_ptr(page);
	pageList.insert(PageList::value_type(pos, page_ptr));
	printDbg(DBG_DBG, "New page added to the pageList size="<<pageList.size())

	return page_ptr;
}

unsigned int CPdf::getPageCount()
{
using namespace utils;
	
	printDbg(DBG_DBG, "");
	
	// gets Pages field from document catalog
	shared_ptr<IProperty> pages_ptr=docCatalog->getProperty("Pages");
	
	// gets type of the dictionary and find out if it is Page node or
	// intermediate node
	shared_ptr<CDict> dict_ptr=getDictFromRef(pages_ptr);
	string dictType=getNameFromDict("Type", dict_ptr);
	if(dictType=="Page")
	{
		// this document contains onlu one page
		printDbg(DBG_INFO, "Page count=1 no intermediate node.");
		return 1;
	}
	if(dictType=="Pages")
	{
		// gets count field
		int count=getIntFromDict("Count", dict_ptr);
		printDbg(DBG_INFO, "Page count="<<count);
		return count;
	}

	printDbg(DBG_CRIT, "Pages dictionary has bad dictionary type type="<<dictType);
	throw MalformedFormatExeption("Pages dictionary is not Page or Pages dictionary");
}

boost::shared_ptr<CPage> CPdf::getNextPage(boost::shared_ptr<CPage> page)
{
	printDbg(DBG_DBG, "");

	size_t pos=getPagePosition(page);
	printDbg(DBG_DBG, "Page position is "<<pos);
	pos++;
	
	// checks if we are in boundary after incrementation
	if(pos>=getPageCount())
	{
		printDbg(DBG_ERR, "Page is out of range pos="<<pos);
		throw PageNotFoundException(pos);
	}

	// page in range, uses getPage
	return getPage(pos);

}

boost::shared_ptr<CPage> CPdf::getPrevPage(boost::shared_ptr<CPage> page)
{
	printDbg(DBG_DBG, "");

	size_t pos=getPagePosition(page);
	printDbg(DBG_DBG, "Page position is "<<pos);
	pos--;
	
	// checks if we are in boundary after incrementation
	if(pos>=getPageCount())
	{
		printDbg(DBG_ERR, "Page is out of range pos="<<pos);
		throw PageNotFoundException(pos);
	}

	// page in range, uses getPage
	return getPage(pos);
}

size_t CPdf::getPagePosition(boost::shared_ptr<CPage> page)
{
	printDbg(DBG_DBG, "");
		
	// search in returned page list
	PageList::iterator i;
	for(i=pageList.begin(); i!=pageList.end(); i++)
	{
		// compares page instances
		// This is ok even if they manage same page dictionary
		if(i->second == page)
		{
			printDbg(DBG_INFO, "Page found at pos="<<i->first);
			return i->first;
		}
	}

	// page not found, it hasn't been returned by this pdf
	throw PageNotFoundException();
}


void CPdf::consolidatePageList(shared_ptr<IProperty> oldValue, shared_ptr<IProperty> newValue)
{
using namespace utils;

	printDbg(DBG_DBG, "");

	// correction for all pages affected by this subtree change
	int difference=0;

	// position of first page which should be considered during consolidation 
	// because of value change
	size_t minPos=0;

	// handles original value - one before change
	// pNull means no previous value available (new sub tree has been added)
	printDbg(DBG_DBG, "oldValue type="<<oldValue->getType());
	if(oldValue->getType()!=pNull)
	{
		// gets dictionary type - it has to be page or pages node
		shared_ptr<CDict> oldDict_ptr=getDictFromRef(oldValue);
		string oldDictType=getNameFromDict("Type", oldDict_ptr);

		// simple page is compared with all from pageList and if found, removes
		// it from list and invalidates it.
		// Difference is set to - 1, because one page is removed 
		if(oldDictType=="Page")
		{
			printDbg(DBG_DBG, "oldValue was simple page dictionary");
			difference = -1;

			PageList::iterator i;
			for(i=pageList.begin(); i!=pageList.end(); i++)
			{
				// checks page's dictionary with old one
				if(i->second->getDictionary() == oldDict_ptr)
				{
					// FIXME: uncoment when method is ready
					//i->second->invalidate();
					size_t pos=i->first;
					minPos=pos;
					pageList.erase(i);
					printDbg(DBG_INFO, "CPage(pos="<<pos<<") associated with oldValue page dictionary removed. pageList.size="<<pageList.size());
					break;
				}
			}
		}else
		{
			// Pages dictionary stands for intermediate node and so all CPages
			// from this sub tree are removed and invalidated
			// difference is set to -Count value (those number of pages are
			// removed)
			if(oldDictType=="Pages")
			{
				printDbg(DBG_DBG, "oldValue was intermediate node dictionary.")
				difference = -getIntFromDict("Count", oldDict_ptr);

				// gets reference of oldValue - which is the root removed
				// subtree
				IndiRef ref;
				IProperty::getSmartCObjectPtr<CRef>(oldValue)->getPropertyValue(ref);
				
				PageList::iterator i;
				for(i=pageList.begin(); i!=pageList.end(); i++)
				{
					// checks page's dictionary whether it is in oldDict_ptr sub
					// tree and if so removes it from pageList
					if(isDescendant(*this, ref, i->second->getDictionary()))
					{
						// updates minPos with page position (if greater)
						size_t pos=i->first;
						if(pos > minPos)
							minPos=pos;
						
						// FIXME: uncoment when method is ready
						//i->second->invalidate();
						pageList.erase(i);
						printDbg(DBG_INFO, "CPage(pos="<<pos<<") associated with oldValue page dictionary removed. pageList.size="<<pageList.size());

					}
				}
			}
		}
	}

	// oldValue subtree (if any) is consolidated now
	printDbg(DBG_DBG, "All page dictionaries from oldValue subtree removed");

	// number of added pages by newValue tree
	int pagesCount=0;
	
	// handles new value - one after change
	// if pNull - no new value is available (subtree has been removed)
	printDbg(DBG_DBG, "newValue type="<<newValue->getType());
	if(newValue->getType()!=pNull)
	{
		// gets dictionary type
		shared_ptr<CDict> newDict_ptr=getDictFromRef(newValue);
		string newDictType=getNameFromDict("Type", newDict_ptr);

		// page type adds only one page
		if(newDictType=="Page")
			pagesCount=1;

		// pages type adds Count pages
		if(newDictType=="Pages")
			pagesCount=getIntFromDict("Count", newDict_ptr);

		// try to get position of newValue node.  No pages from this sbtree can
		// be in the pageList, so we can set minPos to its position.
		// If getNodePosition throws, then this node is ambigues and so we have
		// no information
		try
		{
			minPos = getNodePosition(*this, newValue);
		}catch(exception &e)
		{
			// position can't be determined
			// no special handling is needed, minPos keeps its value
		}

		printDbg(DBG_DBG, "newValue has "<<pagesCount<<" page dictionaries");
	}

	// corrects difference with added pages
	difference -= pagesCount;

	// no difference means no speacial handling for other pages
	// we have replaced old sub tree with new subtree with same number of pages
	if(difference==0)
		return;

	printDbg(DBG_INFO, "pageList consolidation from minPos="<<minPos);
	
	// all pages with position greater than minPos, has to be consolidated
	PageList::iterator i;
	PageList readdContainer;
	for(i=pageList.begin(); i!=pageList.end(); i++)
	{
		// all pages > minPos are removed and readded with correct position
		if(i->first > minPos)
		{
			// collects all removed
			readdContainer.insert(PageList::value_type(i->first, i->second));	
			pageList.erase(i);
		}
	}
	
	// checks minPos==0 and if so, we have to handle situation special way,
	// because don't have any information about previous position of oldValue
	// subtree. In such case all pages which should be readded are searched in
	// page tree and actual position is used.
	if(!minPos)
	{
		printDbg(DBG_INFO, "minPos==0 page position has to be retrieved from page tree.");
		for(i=readdContainer.begin(); i!=readdContainer.end(); i++)
		{
			// uses getNodePosition for each page's dictionary to find out
			// current position. If getNodePosition throws an exception, it
			// means that it can't be determined. Such page is invalidated.
			try
			{
				size_t pos=getNodePosition(*this, i->second->getDictionary());
				pageList.insert(PageList::value_type(pos, i->second));	
			}catch(exception & e)
			{
				// page position is ambigues and so it has to be invalidate
				//TODO uncoment when method is ready
				//i->second->invalidate();
			}
		}
		return;
	}
	
	printDbg(DBG_INFO, "Moving pages position with difference="<<difference);
	// Information about page numbers which should be consolidated is available
	// so just adds difference for each in readdContainer
	// readds all removed with changed position (according difference)
	for(i=readdContainer.begin(); i!=readdContainer.end(); i++)
		pageList.insert(PageList::value_type(i->first+difference, i->second));	
}


void CPdf::consolidatePageTree(boost::shared_ptr<CDict> interNode)
{
using namespace utils;

	printDbg(DBG_DBG, "");

	shared_ptr<IProperty> kidsArrayProp_ptr=interNode->getProperty("Kids");
	if(kidsArrayProp_ptr->getType()!=pArray)
	{
		printDbg(DBG_CRIT, "interNode's Kids field is not an array");
		// Kids field value must be aray
		throw ElementBadTypeException("Kids");
	}

	// gets all children from array
	shared_ptr<CArray> kidsArray_ptr=IProperty::getSmartCObjectPtr<CArray>(kidsArrayProp_ptr);
	vector<shared_ptr<IProperty> > kidsContainer;
	kidsArray_ptr->_getAllChildObjects(kidsContainer);

	// iterate through all children, dereference all of them and checks
	// dictionary type. This used to get information about page count under this
	// node (Page is 1 and Pages is Count)
	size_t count=0;
	vector<shared_ptr<IProperty> >::iterator i;
	size_t index=0;
	for(i=kidsContainer.begin(); i!=kidsContainer.end(); i++, index++)
	{
		if((*i)->getType()!=pRef)
		{
			printDbg(DBG_ERR, "Kids["<<index<<"] element must be reference");
			// each element must be reference
			throw ElementBadTypeException("Kids");
		}

		shared_ptr<CDict> kidDict_ptr=getDictFromRef(*i);
		string dictType=getNameFromDict("Type", kidDict_ptr);
		if(dictType=="Page")
			count++;
		else
			if(dictType=="Pages")
				count+=getIntFromDict("Count", kidDict_ptr);
			else
			{
				printDbg(DBG_WARN, " interNode contains reference to dictionary with bad type="<<dictType);
				// bad dictionaty type is skipped
				continue;
			}
		
		try
		{
			IndiRef parentRef;
			parentRef=getRefFromDict("Parent", kidDict_ptr);
			if(! (parentRef==interNode->getIndiRef()))
			{
				printDbg(DBG_WARN, "Kids["<<index<<"] element dictionary doesn't have Parent with proper reference. Correcting to ref=["<<parentRef.num<<", "<<parentRef.gen<<"]");
				CRef cref(interNode->getIndiRef());
				kidDict_ptr->setProperty("Parent", cref);
			}
			
		}catch(ElementNotFoundException & e)
		{
			// Parent not found at all
			// field is added
			IndiRef parentRef=interNode->getIndiRef();
			printDbg(DBG_WARN, "No Parent field found. Correcting to ref=["<<parentRef.num<<", "<<parentRef.gen<<"]");
			CRef cref(parentRef);
			kidDict_ptr->addProperty("Parent", cref);
		}catch(ElementBadTypeException & e)
		{
			// Parent is found but with bad type
			// Parent field is removed in first step and than added with correct
			// this is because of different type of field value which may lead
			// to exception if types are checked
			IndiRef parentRef=interNode->getIndiRef();
			printDbg(DBG_WARN, "Parent field found but with bad type. Correcting to ref=["<<parentRef.num<<", "<<parentRef.gen<<"]");
			kidDict_ptr->delProperty("Parent");
			CRef cref(parentRef);
			kidDict_ptr->addProperty("Parent", cref);
		}

	}

	// count is collected for this node, checks if it matches actual value and 
	// if not sets new value and distribute it to all parents
	try
	{
		size_t currentCount=(size_t)getIntFromDict("Count", interNode);
		if(currentCount == count)
		{
			// count field is ok - no parent consolidation has to be done
			return;
		}
	}catch(CObjectException &e)
	{
		// not foud or bad type
		// no special handling - just continues as if no bad Count value was
		// present - This should never happen
		printDbg(DBG_WARN, "Intermediate node without or with bad typed Count field.");
	}
	CInt countProp(count);
	printDbg(DBG_INFO, "new interNode Count="<<count);
	interNode->setProperty("Count", countProp);
	
	// distribute value also to fathers - if any
	try
	{
		shared_ptr<IProperty> parentRef_ptr=interNode->getProperty("Parent");
		shared_ptr<CDict> parentDict_ptr=getDictFromRef(parentRef_ptr);

		// calls recursively
		printDbg(DBG_DBG, "Distributing count value to parent");
		consolidatePageTree(parentDict_ptr);
	}
	catch(exception &e)
	{
		// no parent in intermediate node means root of page tree
	}
}

boost::shared_ptr<CPage> CPdf::insertPage(boost::shared_ptr<CPage> page, size_t pos)
{
using namespace utils;

	printDbg(DBG_DBG, "pos="<<pos);

	if(mode==ReadOnly)
	{
		printDbg(DBG_ERR, "Document is opened in read-only mode");
		throw ReadOnlyDocumentException("Document is in read-only mode.");
	}
		
	// zero position is corrected to 1
	if(pos==0)
		pos=1;

	// gets page's dictionary and adds it as new indirect property.
	// All page dictionaries must be indirect objects and addIndirectProperty
	// method also solves problems with deep copy and page from different file
	// transition
	IndiRef pageRef=addIndirectProperty(page->getDictionary());

	// gets intermediate node which includes node at given position. To enable
	// also to insert after last page, following work around is done:
	// if page is greater than page count, append flag is set to true and so new
	// dictionary is stored after last position (instead of storePostion).
	size_t count=getPageCount();
	size_t storePostion=pos;
	bool append=false;
	if(pos>count)
	{
		// sets that we are appending and sets storePostion to last position
		append=true;
		storePostion=count;
		printDbg(DBG_INFO, "inserting after (new last page) position="<<storePostion);
	}

	// search for page at storePostion and gets its reference
	// page dictionary has to be an indirect object, so getIndiRef returns
	// dictionary reference
	shared_ptr<CDict> currentPage_ptr=findPageDict(*this, docCatalog->getProperty("Pages"), 1, storePostion);
	CRef currRef(currentPage_ptr->getIndiRef());
	
	// gets parent of found dictionary which maintains Kids array
	// FIXME this may be problem - because there may be only one page in
	// document which doesn't have any parent
	shared_ptr<IProperty> parentRef_ptr=currentPage_ptr->getProperty("Parent");
	shared_ptr<CDict> interNode_ptr=getDictFromRef(parentRef_ptr);
	shared_ptr<IProperty> kidsProp_ptr=interNode_ptr->getProperty("Kids");
	if(kidsProp_ptr->getType()!=pArray)
	{
		printDbg(DBG_CRIT, "Pages Kids field is not an array type="<<kidsProp_ptr->getType());
		// Kids is not array - malformed intermediate node
		throw MalformedFormatExeption("Intermediate node Kids field is not an array.");
	}
	shared_ptr<CArray> kids_ptr=IProperty::getSmartCObjectPtr<CArray>(kidsProp_ptr);

	// gets index of searched node in Kids array - if position can't be 
	// determined unambiguesly (getPropertyId returns more positions), 
	// (TODO which) exception is thrown
	size_t kidsIndex=0;
	// TODO kidsIndex=kids_ptr->getPropertyId(currentPage_ptr)+append;

	// if we are storing new last page (append is true) we will add one
	storePostion+=append;

	// adds newly created page dictionary to the kids array at kidsIndex
	// position.
	CRef pageCRef(pageRef);
	kids_ptr->addProperty(kidsIndex, pageCRef);
	
	// page dictionary is stored in the tree, consolidation is also done at this
	// moment
	// CPage can be created and inserted to the pageList
	shared_ptr<CDict> newPageDict_ptr=IProperty::getSmartCObjectPtr<CDict>(getIndirectProperty(pageRef));
	shared_ptr<CPage> newPage_ptr(CPageFactory::getInstance(newPageDict_ptr));
	pageList.insert(PageList::value_type(storePostion, newPage_ptr));
	printDbg(DBG_DBG, "New page added to the pageList size="<<pageList.size())
	return newPage_ptr;
}

void CPdf::removePage(size_t pos)
{
using namespace utils;

	printDbg(DBG_DBG, "");

	if(mode==ReadOnly)
	{
		printDbg(DBG_ERR, "Document is opened in read-only mode");
		throw ReadOnlyDocumentException("Document is in read-only mode.");
	}

	// checks position
	if(1<pos || pos>getPageCount())
		throw PageNotFoundException(pos);

	// Searches for page dictionary at given pos and gets its reference.
	shared_ptr<CDict> currentPage_ptr=findPageDict(*this, docCatalog->getProperty("Pages"), 1, pos);
	CRef currRef(currentPage_ptr->getIndiRef());
	
	// Gets parent field from found page dictionary and gets its Kids array
	// FIXME this may be problem - because there may be only one page in
	// document which doesn't have any parent
	shared_ptr<IProperty> parentRef_ptr=currentPage_ptr->getProperty("Parent");
	shared_ptr<CDict> interNode_ptr=getDictFromRef(parentRef_ptr);
	shared_ptr<IProperty> kidsProp_ptr=interNode_ptr->getProperty("Kids");
	if(kidsProp_ptr->getType()!=pArray)
	{
		printDbg(DBG_CRIT, "Pages Kids field is not an array type="<<kidsProp_ptr->getType());
		// Kids is not array - malformed intermediate node
		throw MalformedFormatExeption("Intermediate node Kids field is not an array.");
	}
	shared_ptr<CArray> kids_ptr=IProperty::getSmartCObjectPtr<CArray>(kidsProp_ptr);

	// gets index of searched node in Kids array and removes element from found
	// position - if position can't be determined unambiguesly (getPropertyId
	// returns more positions), (TODO which) exception is thrown
	shared_ptr<CRef> currentRef_ptr(CRefFactory::getInstance(currentPage_ptr->getIndiRef()));
	// TODO size_t kidsIndex=kids_ptr->getPropertyId(currentRef_ptr);
	// kids_ptr->delProperty(kidsIndex);
	
	// page dictionary is removed from the tree, consolidation is done also for
	// pageList at this moment
}

void CPdf::save(bool newRevision)
{
	printDbg(DBG_DBG, "");

	if(mode==ReadOnly)
	{
		printDbg(DBG_ERR, "Document is opened in read-only mode");
		throw ReadOnlyDocumentException("Document is in read-only mode.");
	}
	
	// checks actual revision
	if(xref->getActualRevision())
		// FIXME throw an exception
		return;
	
	// we are in the newest revision, so changes can be saved
	// delegates all work to the XRefWriter and set change to 
	// mark, that no changes were stored
	xref->saveChanges(newRevision);
}

void CPdf::clone(FILE * fname)const
{
	throw NotImplementedException("CPdf::clone");
	// copies everything until xref->getLastXRefPos
	// copy also xref and trailer
}

} // end of pdfobjects namespace
