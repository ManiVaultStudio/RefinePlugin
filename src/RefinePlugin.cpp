#include "RefinePlugin.h"

#include <DataHierarchyItem.h>
#include <event/Event.h>

#include <QDebug>
#include <QGridLayout>
#include <QWidget>

Q_PLUGIN_METADATA(IID "studio.manivault.RefinePlugin")

using namespace mv;

RefinePlugin::RefinePlugin(const PluginFactory* factory) :
    plugin::ViewPlugin(factory),
    _hsnePoints(nullptr),
    _scatterplotView(nullptr),
    _refineAction(this, "Refine"),
    _datasetPickerAction(this, "Dataset"),
    _updateDatasetAction(this, "Focus on refinement"),
    _scatterplotAction(this, "Attach to")
{
    _scatterplotAction.setToolTip("Data opens in a new scatteplot. \nThe new scatterplot can be opened as a tab atatched to an existing one.");
    _updateDatasetAction.setToolTip("When refining a selection, focus the refine button on the newly created data set");

    _datasetPickerAction.setFilterFunction([](const mv::Dataset<DatasetImpl> dataset) -> bool {

        // Only list HSNE embeddings and refined scales
        if (!dataset->isVisible())
            return false;

        if (dataset->getDataType() != PointType)
            return false;

        if (!dataset->isDerivedData())
            return false;

        const QString datasetName = dataset->getGuiName();

        // do not add lowest scale
        if (datasetName.contains("Hsne scale 0", Qt::CaseInsensitive))
            return false;

        const QString refineActionPath = "HSNE Scale/Refine selection";

        if (dataset->findChildByPath(refineActionPath) ||
            dataset->getParent()->findChildByPath(refineActionPath) // extra check as sometimes the action is only added after this check
            ) {
            return true;
        }

        return false;
    });

    auto resetScatterplotOptions = [this]() {
        _scatterplotAction.setOptions(getScatterplotOptions());
        _scatterplotAction.setCurrentIndex(0);
        _scatterplotAction.setEnabled(true);
        };

    auto updateScatterplotOptions = [this, resetScatterplotOptions]() {
        const auto currentOption = _scatterplotAction.getCurrentText();
        resetScatterplotOptions();

        if (_scatterplotAction.hasOption(currentOption))
            _scatterplotAction.setCurrentText(currentOption);
        };

    connect(&mv::plugins(), &AbstractPluginManager::pluginAdded, this, [this, updateScatterplotOptions](plugin::Plugin* plugin) -> void {
        updateScatterplotOptions();
        });

    connect(&mv::plugins(), &AbstractPluginManager::pluginDestroyed, this, [this, updateScatterplotOptions](const QString& id) -> void {
        updateScatterplotOptions();
        });

    connect(&_refineAction, &gui::TriggerAction::triggered, this, &RefinePlugin::onRefine);

    connect(&_datasetPickerAction, &gui::DatasetPickerAction::datasetPicked, this, [this](mv::Dataset<mv::DatasetImpl> newData) {
        if (newData->getDataType() != PointType)
            return;

        _hsnePoints = newData;
        });

    _eventListener.addSupportedEventType(static_cast<std::uint32_t>(EventType::DatasetAdded));
    _eventListener.registerDataEventByType(PointType, std::bind(&RefinePlugin::onDataEvent, this, std::placeholders::_1));

    resetScatterplotOptions();
}

std::vector<mv::plugin::Plugin*> RefinePlugin::getOpenScatterplots()
{
    std::vector<mv::plugin::Plugin*> openScatterplots;

    for (plugin::Plugin* openPlugin : mv::plugins().getPluginsByType(plugin::Type::VIEW))
        if (openPlugin->getKind() == "Scatterplot View")
            openScatterplots.push_back(openPlugin);

    return openScatterplots;
}

QStringList RefinePlugin::getScatterplotOptions()
{
    QStringList scatterplotOptions = { "New scatterplot" };

    for (const mv::plugin::Plugin* scatterplot : getOpenScatterplots())
        scatterplotOptions << scatterplot->getGuiName();

    assert(scatterplotOptions.size() > 0);

    return scatterplotOptions;
}

void RefinePlugin::init()
{
    QWidget* viewWidget = &getWidget();

    // Create layout
    auto layout = new QGridLayout();
    layout->setContentsMargins(5, 0, 5, 0);

    QWidget* refineButton = _refineAction.createWidget(viewWidget);
    {
        QWidget* refineButtonWidget = refineButton->layout()->itemAt(0)->widget();
        refineButtonWidget->setFixedHeight(100);

        QFont font = refineButtonWidget->font();
        font.setPointSize(48);
        refineButtonWidget->setFont(font);

        refineButtonWidget->setStyleSheet("font-size: 48px;");
    }

    layout->addWidget(refineButton,                                         0, 0, 2, 6);
    layout->addWidget(_datasetPickerAction.createLabelWidget(viewWidget),   2, 0, 1, 1);
    layout->addWidget(_datasetPickerAction.createWidget(viewWidget),        2, 1, 1, 1);
    layout->addWidget(_updateDatasetAction.createWidget(viewWidget),        2, 3, 1, 1);
    layout->addWidget(_scatterplotAction.createLabelWidget(viewWidget),     2, 4, 1, 1);
    layout->addWidget(_scatterplotAction.createWidget(viewWidget),          2, 5, 1, 1);

    viewWidget->setLayout(layout);
}

void RefinePlugin::loadData(const Datasets& datasets)
{
    // Exit if there is nothing to load
    if (datasets.isEmpty())
        return;

    if (!_datasetPickerAction.hasOption(datasets.first()->getGuiName()))
        return;

    _datasetPickerAction.setCurrentDataset(datasets.first()->getId());
}

void RefinePlugin::onDataEvent(mv::DatasetEvent* dataEvent)
{
    if (!_hsnePoints.isValid())
    {
        qDebug() << "RefinePlugin::onDataEvent: data set is invalid";
        return;
    }

    switch (dataEvent->getType()) {
    case EventType::DatasetAdded:
    {
        const auto changedDataSet = dataEvent->getDataset();

        // add new refined embedding to new scatterplot
        if (changedDataSet->getGuiName().contains("Hsne scale") &&
            changedDataSet->getDataHierarchyItem().getParent()->getDataset().get<Points>()->getId() == _hsnePoints->getId())
        {
            // get potential parent of new scatterplot
            mv::plugin::ViewPlugin* parentView = nullptr;
            mv::gui::DockAreaFlag dockArea = gui::DockAreaFlag::Right;

            if (_scatterplotAction.getCurrentText() != "New scatterplot")
            {
                for (mv::plugin::Plugin* openScatterplot : getOpenScatterplots())
                    if (openScatterplot->getGuiName() == _scatterplotAction.getCurrentText())
                        parentView = dynamic_cast<mv::plugin::ViewPlugin*>(openScatterplot);

                if (parentView != nullptr)
                    dockArea = mv::gui::DockAreaFlag::Center;
            }

            // open new scatterplot
            _scatterplotView = mv::plugins().requestViewPlugin("Scatterplot View", parentView, dockArea);
            _scatterplotView->loadData({ changedDataSet });

            if (_updateDatasetAction.isChecked() && !changedDataSet->getGuiName().contains("Hsne scale 0"))
            {
                if (_datasetPickerAction.getDatasets().contains(changedDataSet))
                    _datasetPickerAction.setCurrentDataset(changedDataSet->getId());
            }
        }
    }
    }

}

void RefinePlugin::onRefine()
{
    if (!_hsnePoints.isValid())
    {
        qDebug() << "No refining since data set is invalid";
        return;
    }

    if (_hsnePoints->getSelectionIndices().empty())
    {
        qDebug() << "No refining since selection is empty in " << _hsnePoints->getGuiName();
        return;
    }

    // top level embedding and scales have different UI layouts
    auto refineAction = _hsnePoints->findChildByPath("HSNE Settings/HSNE Scale/Refine selection");
    if(refineAction == nullptr)
        refineAction = _hsnePoints->findChildByPath("HSNE Scale/Refine selection");

    if (refineAction != nullptr)
    {
        qDebug() << "Refine selection in " << _hsnePoints->getGuiName();
        gui::TriggerAction* refineTriggerAction = dynamic_cast<gui::TriggerAction*>(refineAction);
        refineTriggerAction->trigger();
    }

}

void RefinePlugin::fromVariantMap(const QVariantMap& variantMap)
{
    ViewPlugin::fromVariantMap(variantMap);

    _refineAction.fromParentVariantMap(variantMap);
    _datasetPickerAction.fromParentVariantMap(variantMap);
    _updateDatasetAction.fromParentVariantMap(variantMap);
    _scatterplotAction.fromParentVariantMap(variantMap);
}

QVariantMap RefinePlugin::toVariantMap() const
{
    QVariantMap variantMap = ViewPlugin::toVariantMap();

    _refineAction.insertIntoVariantMap(variantMap);
    _datasetPickerAction.insertIntoVariantMap(variantMap);
    _updateDatasetAction.insertIntoVariantMap(variantMap);
    _scatterplotAction.insertIntoVariantMap(variantMap);

    return variantMap;
}


// =============================================================================
// Factory
// =============================================================================

RefinePluginFactory::RefinePluginFactory() {
    setIconByName("filter");
}

ViewPlugin* RefinePluginFactory::produce()
{
    return new RefinePlugin(this);
}

mv::DataTypes RefinePluginFactory::supportedDataTypes() const
{
    return { PointType };
}

mv::gui::PluginTriggerActions RefinePluginFactory::getPluginTriggerActions(const mv::Datasets& datasets) const
{
    gui::PluginTriggerActions pluginTriggerActions;

    const auto getPluginInstance = [this]() -> RefinePlugin* {
        return dynamic_cast<RefinePlugin*>(plugins().requestViewPlugin(getKind()));
    };

    const auto numberOfDatasets = datasets.count();

    if (numberOfDatasets >= 1 && PluginFactory::areAllDatasetsOfTheSameType(datasets, PointType)) {
        auto pluginTriggerAction = new gui::PluginTriggerAction(const_cast<RefinePluginFactory*>(this), this, "Refine", "Refine HSNE data", icon(), [this, getPluginInstance, datasets](gui::PluginTriggerAction& pluginTriggerAction) -> void {
            for (const auto& dataset : datasets)
                getPluginInstance()->loadData(Datasets({ dataset }));;
        });

        pluginTriggerActions << pluginTriggerAction;
    }

    return pluginTriggerActions;
}
