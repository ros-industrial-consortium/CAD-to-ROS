#include "urdf_editor/urdf_property_tree.h"
#include "urdf_editor/urdf_property_tree_link_item.h"
#include "urdf_editor/urdf_property_tree_joint_item.h"
#include "urdf/model.h"
#include "ros/ros.h"

namespace urdf_editor
{
  const QString ACTION_ADD_TEXT = "Add";
  const QString ACTION_REMOVE_TEXT = "Remove";
  const QString ACTION_EXPANDALL_TEXT = "Expand All";
  const QString ACTION_COLLAPSEALL_TEXT = "Collapse All";

  URDFPropertyTree::URDFPropertyTree(QWidget *parent) :
    drag_item_(NULL),
    model_(new urdf::ModelInterface())
  {
    setSelectionMode(QAbstractItemView::SingleSelection);
    setDragEnabled(true);
    viewport()->setAcceptDrops(true);
    setDropIndicatorShown(true);
    setDragDropMode(QAbstractItemView::InternalMove);
    setColumnCount(1);

    initialize();
    createActions();
    createMenus();

    connect(this, SIGNAL(itemPressed(QTreeWidgetItem*,int)),
            this, SLOT(on_itemPressed(QTreeWidgetItem*,int)));

    connect(this, SIGNAL(customContextMenuRequested(QPoint)),
            this, SLOT(on_contextMenuRequested(QPoint)));
  }

  URDFPropertyTree::~URDFPropertyTree()
  {
    delete context_menu_;
  }

  bool URDFPropertyTree::loadRobotModel(urdf::ModelInterfaceSharedPtr model)
  {

    if (!model)
      return false;

    model_ = model;

    return populateFromRobotModel();
  }

  urdf::ModelInterfaceSharedPtr URDFPropertyTree::getRobotModel()
  {
    return model_;
  }

  QTreeWidgetItem * URDFPropertyTree::getSelectedItem()
  {
    return selectedItems()[0];
  }

  bool URDFPropertyTree::isJoint(QTreeWidgetItem *item)
  {
    return (item->type() == URDFPropertyTree::Joint ? true : false);
  }

  bool URDFPropertyTree::isLink(QTreeWidgetItem *item)
  {
    return (item->type() == URDFPropertyTree::Link ? true : false);
  }

  bool URDFPropertyTree::isJointRoot(QTreeWidgetItem *item)
  {
    return (item->type() == URDFPropertyTree::JointRoot ? true : false);
  }

  bool URDFPropertyTree::isLinkRoot(QTreeWidgetItem *item)
  {
    return (item->type() == URDFPropertyTree::LinkRoot ? true : false);
  }

  URDFPropertyTreeLinkItem *URDFPropertyTree::asLinkTreeItem(QTreeWidgetItem *item)
  {
    if (isLink(item))
    {
      return dynamic_cast<URDFPropertyTreeLinkItem *>(item);
    }
    else
    {
      ROS_ERROR("Tried to convert a QTreeWidgetItem to a URDFPropertyTreeLinkItem that is not a URDFPropertyTreeLinkItem.");
      return NULL;
    }
  }

  URDFPropertyTreeJointItem *URDFPropertyTree::asJointTreeItem(QTreeWidgetItem *item)
  {
    if (isJoint(item))
    {
      return dynamic_cast<URDFPropertyTreeJointItem *>(item);
    }
    else
    {
      ROS_ERROR("Tried to convert a QTreeWidgetItem to a URDFPropertyTreeJointItem that is not a URDFPropertyTreeJointItem.");
      return NULL;
    }
  }

  void URDFPropertyTree::clear()
  {
    // Clear Links from tree
    while (link_root_->childCount() > 0)
      link_root_->removeChild(link_root_->child(0));

    // Clear Joints from tree
    while (joint_root_->childCount() > 0)
      joint_root_->removeChild(joint_root_->child(0));

    link_names_.clear();
    joint_names_.clear();
    links_.clear();
    joints_.clear();
  }

  void URDFPropertyTree::initialize()
  {
    root_ = new QTreeWidgetItem(this);
    root_->setText(0, "RobotModel");
    root_->setExpanded(true);
    root_->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    addTopLevelItem(root_);

    link_root_ = new QTreeWidgetItem(URDFPropertyTree::LinkRoot);
    link_root_->setText(0, "Links");
    link_root_->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);

    root_->addChild(link_root_);

    joint_root_ = new QTreeWidgetItem(URDFPropertyTree::JointRoot);
    joint_root_->setText(0,"Joints");
    joint_root_->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    root_->addChild(joint_root_);
  }

  void URDFPropertyTree::createMenus()
  {
    context_menu_ = new QMenu(this);
    context_menu_->addAction(add_action_);
    context_menu_->addAction(remove_action_);
    context_menu_->addSeparator();
    context_menu_->addAction(expand_action_);
    context_menu_->addAction(collapse_action_);
  }

  void URDFPropertyTree::createActions()
  {
    add_action_ = new QAction(ACTION_ADD_TEXT, this);
    add_action_->setStatusTip(tr("Add new link to selected link."));
    connect(add_action_,SIGNAL(triggered()), this, SLOT(on_addActionTriggered()));

    remove_action_ = new QAction(ACTION_REMOVE_TEXT, this);
    remove_action_->setStatusTip(tr("Remove selected link."));
    connect(remove_action_,SIGNAL(triggered()), this, SLOT(on_removeActionTriggered()));

    collapse_action_ = new QAction(ACTION_COLLAPSEALL_TEXT, this);
    collapse_action_->setStatusTip(tr("Collapse selected item and all its children."));
    connect(collapse_action_,SIGNAL(triggered()), this, SLOT(on_collapseActionTriggered()));

    expand_action_ = new QAction(ACTION_EXPANDALL_TEXT, this);
    expand_action_->setStatusTip(tr("Expand selected item and all its children."));
    connect(expand_action_,SIGNAL(triggered()), this, SLOT(on_expandActionTriggered()));
  }

  bool URDFPropertyTree::populateFromRobotModel()
  {
    clear();

    // add all links to the tree, starting with the root
    urdf::LinkSharedPtr rlink;
    model_->getLink(model_->getRoot()->name, rlink);
    addItemRecursively(rlink, link_root_);

    // add all joints, starting with those that have the root as parent
    std::vector<urdf::JointSharedPtr>& child_joints = rlink->child_joints;
    std::vector<urdf::JointSharedPtr>::iterator joint_it;

    for (joint_it = child_joints.begin(); joint_it != child_joints.end(); ++joint_it)
      addItemRecursively(*joint_it, joint_root_);

    return true;
  }

  void URDFPropertyTree::addItemRecursively(urdf::LinkSharedPtr link, QTreeWidgetItem *parent)
  {
    // first add the tree item
    QTreeWidgetItem* item = addLinkTreeItem(parent, link);

    // now do child links
    std::vector<urdf::LinkSharedPtr >& child_links = link->child_links;
    std::vector<urdf::LinkSharedPtr >::iterator link_it;

    for (link_it = child_links.begin(); link_it != child_links.end(); ++link_it)
      addItemRecursively(*link_it, item);  // recursive
  }

  void URDFPropertyTree::addItemRecursively(urdf::JointSharedPtr joint, QTreeWidgetItem *parent)
  {
        // see which joints are the children of the child_link
    urdf::LinkSharedPtr child_link;
    model_->getLink(joint->child_link_name, child_link);

    if (child_link)
    {
      // first add the tree item
      URDFPropertyTreeJointItem *item = addJointTreeItem(parent, joint);

      // assign joint to link
      links_[QString::fromStdString(joint->child_link_name)]->assignJoint(item);

      std::vector<urdf::JointSharedPtr >& child_joints = child_link->child_joints;
      std::vector<urdf::JointSharedPtr >::iterator joint_it;

      for (joint_it = child_joints.begin(); joint_it != child_joints.end(); ++joint_it)
        addItemRecursively(*joint_it, item);  // recursive
    }
    else
    {
      qDebug() << QString("Can't find Link object for child_link '%s' of '%s'").arg(
        joint->child_link_name.c_str(), joint->name.c_str());
    }
  }

  urdf::LinkSharedPtr URDFPropertyTree::addModelLink()
  {
    // add link to urdf model
    QString name = getValidName("link_", link_names_);
    urdf::LinkSharedPtr new_link(new urdf::Link());
    new_link->name = name.toStdString();
    model_->links_.insert(std::make_pair(name.toStdString(), new_link));

    // If this is the first link added set it as the root link.
    if (model_->links_.size() == 1)
      model_->root_link_ = new_link;

    return new_link;
  }

  URDFPropertyTreeLinkItem* URDFPropertyTree::addLinkTreeItem(QTreeWidgetItem *parent, urdf::LinkSharedPtr link)
  {
    URDFPropertyTreeLinkItem* item = new URDFPropertyTreeLinkItem(link, link_names_);
    connect(item, SIGNAL(linkNameChanged(URDFPropertyTreeLinkItem*)),
            this, SLOT(on_linkNameChanged(URDFPropertyTreeLinkItem*)));
    connect(item, SIGNAL(valueChanged()), this, SIGNAL(propertyValueChanged()));

    // Map link name to tree widget item
    links_.insert(QString::fromStdString(link->name), item);
    link_names_.append(QString::fromStdString(link->name));

    parent->addChild(item);
    parent->setExpanded(true);
    return item;
  }

  void URDFPropertyTree::removeModelLink(urdf::LinkSharedPtr link)
  {
    model_->links_.erase(model_->links_.find(link->name));
  }

  void URDFPropertyTree::removeLinkTreeItem(QTreeWidgetItem *item)
  {
    URDFPropertyTreeLinkItem *link = asLinkTreeItem(item);
    // Remove mapping between link name and tree widget item.
    links_.remove(QString::fromStdString(link->getData()->name));
    link_names_.removeOne(QString::fromStdString(link->getData()->name));

    QTreeWidgetItem *parent = link->parent();
    moveTreeChildren(item, parent);
    parent->removeChild(item);
  }

  urdf::JointSharedPtr URDFPropertyTree::addModelJoint(QString child_link_name)
  {
    // add joint to urdf model
    QString name = getValidName("joint_", joint_names_);
    urdf::JointSharedPtr new_joint(new urdf::Joint());
    new_joint->name = name.toStdString();
    new_joint->child_link_name = child_link_name.toStdString();
    model_->joints_.insert(std::make_pair(name.toStdString(), new_joint));

    return new_joint;
  }

  URDFPropertyTreeJointItem* URDFPropertyTree::addJointTreeItem(QTreeWidgetItem *parent, urdf::JointSharedPtr joint)
  {
    if (isJointRoot(parent))
      joint->parent_link_name = model_->getRoot()->name;
    else
      joint->parent_link_name = asJointTreeItem(parent)->getPropertyData()->getChildLinkName().toStdString();

    URDFPropertyTreeJointItem* item = new URDFPropertyTreeJointItem(joint, link_names_, joint_names_);
    item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);

    connect(item, SIGNAL(jointNameChanged(URDFPropertyTreeJointItem*)),
            this, SLOT(on_jointNameChanged(URDFPropertyTreeJointItem*)));
    connect(item, SIGNAL(parentLinkChanged(URDFPropertyTreeJointItem*)),
            this, SLOT(on_jointParentLinkChanged(URDFPropertyTreeJointItem*)));
    connect(item, SIGNAL(valueChanged()), this, SIGNAL(propertyValueChanged()));

    // Map link name to tree widget item
    joints_.insert(QString::fromStdString(joint->name), item);
    joint_names_.append(QString::fromStdString(joint->name));

    parent->addChild(item);
    parent->setExpanded(true);
    return item;
  }

  void URDFPropertyTree::removeModelJoint(urdf::JointSharedPtr joint)
  {
    model_->joints_.erase(model_->joints_.find(joint->name));
  }

  void URDFPropertyTree::removeJointTreeItem(QTreeWidgetItem *item)
  {
    URDFPropertyTreeJointItem* joint = asJointTreeItem(item);
    QString newParentName = joint->getPropertyData()->getParentLinkName();

    // Remove mapping between joint name and tree widget item.
    links_.remove(QString::fromStdString(joint->getData()->name));
    joint_names_.removeOne(QString::fromStdString(joint->getData()->name));

    // If it is trying to set the parent to a link that does not exits,
    // set the parent to the firts link in the chain. This should only
    // occur if the user delets the firts link
    if (!link_names_.contains(newParentName))
      newParentName = QString::fromStdString(model_->getRoot()->name);

    QTreeWidgetItem *parent = item->parent();

    for (int i=0; i < item->childCount(); i++)
    {
      asJointTreeItem(item->child(i))->getPropertyData()->setParentLinkName(newParentName);
    }

    parent->removeChild(item);
  }

  QString URDFPropertyTree::getValidName(QString prefix, QStringList &current_names)
  {
    int i = 0;
    QString name;
    do
    {
      i+=1;
      name = prefix + QString::number(i);
    } while (current_names.contains(name));
    return name;
  }

  void URDFPropertyTree::setExpandedRecursive(QTreeWidgetItem *item, bool expanded)
  {
    item->setExpanded(expanded);
    for (int i=0; i < item->childCount(); i++)
      setExpandedRecursive(item->child(i), expanded);
  }

  void URDFPropertyTree::moveTreeChildren(QTreeWidgetItem *parent, QTreeWidgetItem *new_parent)
  {
    for (int i=0; i < parent->childCount(); i++)
      new_parent->addChild(parent->takeChild(i));
  }

  void URDFPropertyTree::on_itemPressed(QTreeWidgetItem *item, int column)
  {
    drag_item_ = item;
  }

  void URDFPropertyTree::on_addActionTriggered()
  {
    QTreeWidgetItem *sel = getSelectedItem();
    urdf::LinkSharedPtr link = addModelLink();
    URDFPropertyTreeLinkItem *new_link = addLinkTreeItem(sel, link);
    emit linkAddition();

    if (link_names_.count() > 2 && !isLinkRoot(sel->parent()))
    {
      URDFPropertyTreeLinkItem *sel_link = asLinkTreeItem(sel);
      URDFPropertyTreeJointItem *parent = sel_link->getAssignedJoint();

      urdf::JointSharedPtr joint = addModelJoint(QString::fromStdString(link->name));
      URDFPropertyTreeJointItem *new_joint = addJointTreeItem(parent, joint);
      new_link->assignJoint(new_joint);
      emit jointAddition();
    }
    else if (link_names_.count() == 2 || isLinkRoot(sel->parent()))
    {
      urdf::JointSharedPtr joint = addModelJoint(QString::fromStdString(link->name));
      URDFPropertyTreeJointItem *new_joint = addJointTreeItem(joint_root_, joint);
      new_link->assignJoint(new_joint);
      emit jointAddition();
    }
  }

  void URDFPropertyTree::on_removeActionTriggered()
  {
    URDFPropertyTreeLinkItem* sel = asLinkTreeItem(getSelectedItem());
    removeModelLink(sel->getData());
    removeLinkTreeItem(sel);
    emit linkDeletion();

    if (sel->hasAssignedJoint())
    {
      removeModelJoint(sel->getAssignedJoint()->getData());
      removeJointTreeItem(sel->getAssignedJoint());
      emit jointDeletion();
    }
  }

  void URDFPropertyTree::on_expandActionTriggered()
  {
    setExpandedRecursive(getSelectedItem(), true);
  }

  void URDFPropertyTree::on_collapseActionTriggered()
  {
    setExpandedRecursive(getSelectedItem(), false);
  }

  void URDFPropertyTree::on_contextMenuRequested(const QPoint &pos)
  {
    if (selectedItems().isEmpty())
      return;

    QTreeWidgetItem *sel = getSelectedItem();

    if (isLinkRoot(sel) && link_names_.isEmpty() || isLink(sel))
    {
      add_action_->setEnabled(true);
      remove_action_->setEnabled(isLink(sel));
      expand_action_->setEnabled(true);
      collapse_action_->setEnabled(true);
    }
    else if (isJointRoot(sel) || isJoint(sel) || (isLinkRoot(sel) && !link_names_.isEmpty()))
    {
      add_action_->setEnabled(false);
      remove_action_->setEnabled(false);
      expand_action_->setEnabled(true);
      collapse_action_->setEnabled(true);
    }

    context_menu_->exec(mapToGlobal(pos));
  }

  void URDFPropertyTree::on_jointNameChanged(URDFPropertyTreeJointItem *joint)
  {
    Q_UNUSED(joint)
  }

  void URDFPropertyTree::on_jointParentLinkChanged(URDFPropertyTreeJointItem *joint)
  {
    URDFPropertyTreeLinkItem *parent_link = links_[joint->getPropertyData()->getParentLinkName()];
    URDFPropertyTreeLinkItem *child_link = links_[joint->getPropertyData()->getChildLinkName()];
    QTreeWidgetItem *newParent;

    // if a parent joint does not exist (in the case of the first link),
    // set it to the root joint QTreeWidgetItem otherwise get the assigned joint.
    if (parent_link->hasAssignedJoint())
      newParent = parent_link->getAssignedJoint();
    else
      newParent = joint_root_;

    QTreeWidgetItem *take = joint->parent()->takeChild(joint->parent()->indexOfChild(joint));
    newParent->addChild(take);

    // move link QTreeWidgetItem
    take = child_link->parent()->takeChild(child_link->parent()->indexOfChild(child_link));
    parent_link->addChild(take);
  }

  void URDFPropertyTree::on_linkNameChanged(URDFPropertyTreeLinkItem *link)
  {
    Q_UNUSED(link)
  }

  void URDFPropertyTree::dropEvent(QDropEvent * event)
  {

    QTreeWidget *tree = (QTreeWidget*)event->source();
    QTreeWidgetItem *item = tree->itemAt(event->pos());

    QModelIndex droppedIndex = indexAt( event->pos() );
    if( !droppedIndex.isValid() )
      return;

    QTreeWidgetItem* drop_item;
    DropIndicatorPosition dp = dropIndicatorPosition();
    // send the appropriate item and drop indicator signal
    if (dp == QAbstractItemView::OnItem || dp == QAbstractItemView::BelowItem)
    {
      drop_item = item;
    }
    else if (dp == QAbstractItemView::AboveItem)
    {
      drop_item = item->parent();
    }

    if (isLink(drag_item_) && isLink(drop_item))
      asLinkTreeItem(drag_item_)->getAssignedJoint()->getPropertyData()->setParentLinkName(drop_item->text(0));

    drop_item->setExpanded(true);
  }
}
