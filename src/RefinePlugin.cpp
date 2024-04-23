#include "RefinePlugin.h"

#include <DataHierarchyItem.h>
#include <event/Event.h>

#include <QDebug>
#include <QGridLayout>
#include <QWidget>

Q_PLUGIN_METADATA(IID "studio.manivault.RefinePlugin")

using namespace mv;

RefinePlugin::RefinePlugin(const PluginFactory* factory) :
    ViewPlugin(factory),
    _points(),
    _scatterplotView(nullptr),
    _refineAction(this, "Refine"),
    _datasetPickerAction(this, "Dataset", DatasetPickerAction::Mode::Automatic),
    _updateDatasetAction(this, "Focus button on refined dataset")
{

    _datasetPickerAction.setDatasetsFilterFunction([](const mv::Datasets& datasets) -> Datasets {
        Datasets possibleInitDataset;

        // Only list HSNE embeddings and refined scales
        for (const auto& dataset : datasets)
        {
            if (!dataset->isVisible())
                continue;

            if (dataset->getDataType() != PointType)
                continue;

             if (!dataset->isDerivedData())
                continue;

             if (dataset->findChildByPath("HSNE Scale/Refine selection") == nullptr)
             {
                 // extra check since for refinements that action is seemingly added after this callback is triggered
                 if (!dataset->getGuiName().contains("Hsne scale") && !dataset->getGuiName().contains("HSNE Embedding"))
                     continue;
             }

             // do not add lowest scale
             if (dataset->getGuiName().contains("Hsne scale 0"))
                 continue;

            possibleInitDataset << dataset;
        }

        return possibleInitDataset;
        });


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

    layout->addWidget(refineButton,                                         0, 0, 2, 4);
    layout->addWidget(_datasetPickerAction.createLabelWidget(viewWidget),   2, 0, 1, 1);
    layout->addWidget(_datasetPickerAction.createWidget(viewWidget),        2, 1, 1, 1);
    layout->addWidget(_updateDatasetAction.createWidget(viewWidget),        2, 2, 1, 1);

    viewWidget->setLayout(layout);

    connect(&_refineAction, &TriggerAction::triggered, this, &RefinePlugin::onRefine);

    connect(&_datasetPickerAction, &DatasetPickerAction::datasetPicked, this, [this](mv::Dataset<mv::DatasetImpl> newData){
        if (newData->getDataType() != PointType)
            return;

        _points = newData;
    });

    connect(&_datasetPickerAction, &DatasetPickerAction::datasetsChanged, this, [this](mv::Datasets newDatasets){

        if (!_updateDatasetAction.isChecked())
            return;

        if(_datasetPickerAction.getDatasets().contains(_points))
            _datasetPickerAction.setCurrentDataset(_points->getId());

    });

    _eventListener.addSupportedEventType(static_cast<std::uint32_t>(EventType::DatasetAdded));
    _eventListener.registerDataEventByType(PointType, std::bind(&RefinePlugin::onDataEvent, this, std::placeholders::_1));
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

    switch (dataEvent->getType()) {
    case EventType::DatasetAdded:
    {
        const auto changedDataSet = dataEvent->getDataset();

        // add new refined embedding to new scatterplot
        if (changedDataSet->getGuiName().contains("Hsne scale") &&
            changedDataSet->getDataHierarchyItem().getParent()->getDataset().get<Points>()->getId() == _points->getId())
        {
            _scatterplotView = mv::plugins().requestViewPlugin("Scatterplot View");

            _scatterplotView->loadData({ changedDataSet });

            if (_updateDatasetAction.isChecked() && !changedDataSet->getGuiName().contains("Hsne scale 0"))
                _points = changedDataSet;
        }
    }
    }
}

void RefinePlugin::onRefine()
{
    if (!_points.isValid())
    {
        qDebug() << "No refining since data set is invalid";
        return;
    }

    if (_points->getSelectionIndices().empty())
    {
        qDebug() << "No refining since selection is empty in " << _points->getGuiName();
        return;
    }

    // top level embedding and scales have different UI layouts
    auto refineAction = _points->findChildByPath("HSNE Settings/HSNE Scale/Refine selection");
    if(refineAction == nullptr)
        refineAction = _points->findChildByPath("HSNE Scale/Refine selection");

    if (refineAction != nullptr)
    {
        qDebug() << "Refine selection in " << _points->getGuiName();
        TriggerAction* refineTriggerAction = dynamic_cast<TriggerAction*>(refineAction);
        refineTriggerAction->trigger();
    }

}

/// ////////////// ///
/// Plugin factory ///
/// ////////////// ///

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
    PluginTriggerActions pluginTriggerActions;

    const auto getPluginInstance = [this]() -> RefinePlugin* {
        return dynamic_cast<RefinePlugin*>(plugins().requestViewPlugin(getKind()));
    };

    const auto numberOfDatasets = datasets.count();

    if (numberOfDatasets >= 1 && PluginFactory::areAllDatasetsOfTheSameType(datasets, PointType)) {
        auto pluginTriggerAction = new PluginTriggerAction(const_cast<RefinePluginFactory*>(this), this, "Refine", "Refine HSNE data", getIcon(), [this, getPluginInstance, datasets](PluginTriggerAction& pluginTriggerAction) -> void {
            for (const auto& dataset : datasets)
                getPluginInstance()->loadData(Datasets({ dataset }));;
        });

        pluginTriggerActions << pluginTriggerAction;
    }

    return pluginTriggerActions;
}
