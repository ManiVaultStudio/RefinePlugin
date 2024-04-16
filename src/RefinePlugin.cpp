#include "RefinePlugin.h"

#include <AnalysisPlugin.h>
#include <event/Event.h>

#include <actions/TriggerAction.h>
#include <actions/DatasetPickerAction.h>

#include <DatasetsMimeData.h>

#include <QDebug>
#include <QMimeData>

#include <chrono>

Q_PLUGIN_METADATA(IID "studio.manivault.RefinePlugin")

using namespace mv;

RefinePlugin::RefinePlugin(const PluginFactory* factory) :
    ViewPlugin(factory),
    _points()
{

}

void RefinePlugin::init()
{
    // Create layout
    auto layout = new QVBoxLayout();

    layout->setContentsMargins(0, 0, 0, 0);

    TriggerAction* refineButton = new TriggerAction(this, "Refine");
    QWidget* refWidget = refineButton->createWidget(&getWidget());
    for (int i = 0; i < refWidget->layout()->count(); ++i)
    {
        QWidget* widget = refWidget->layout()->itemAt(i)->widget();
        if (widget != NULL)
        {
            widget->setFixedHeight(100);

            QFont font = widget->font();
            font.setPointSize(48);
            widget->setFont(font);

            widget->setStyleSheet("font-size: 48px;");
            //widget->setVisible(false);
        }
        else
        {
            // You may want to recurse, or perform different actions on layouts.
            // See gridLayout->itemAt(i)->layout()
        }
    }
    layout->addWidget(refWidget);

    connect(refineButton, &TriggerAction::triggered, this, &RefinePlugin::onRefine);

    // Apply the layout
    getWidget().setLayout(layout);

    // Respond when the name of the dataset in the dataset reference changes
    connect(&_points, &Dataset<Points>::guiNameChanged, this, [this]() {

        auto newDatasetName = _points->getGuiName();
    });

    // Alternatively, classes which derive from hdsp::EventListener (all plugins do) can also respond to events
    _eventListener.addSupportedEventType(static_cast<std::uint32_t>(EventType::DatasetAdded));

    _eventListener.registerDataEventByType(PointType, std::bind(&RefinePlugin::onDataEvent, this, std::placeholders::_1));
}

void RefinePlugin::loadData(const Datasets& datasets)
{
    // Exit if there is nothing to load
    if (datasets.isEmpty())
        return;

    // Load the first dataset
    _points = datasets.first();
}

void RefinePlugin::onDataEvent(mv::DatasetEvent* dataEvent)
{
    // Get smart pointer to dataset that changed
    const auto changedDataSet = dataEvent->getDataset();

    // Get GUI name of the dataset that changed
    const auto datasetGuiName = changedDataSet->getGuiName();

    // The data event has a type so that we know what type of data event occurred (e.g. data added, changed, removed, renamed, selection changes)
    switch (dataEvent->getType()) {

        // A points dataset was added
        case EventType::DatasetAdded:
        {
            // Cast the data event to a data added event
            const auto dataAddedEvent = static_cast<DatasetAddedEvent*>(dataEvent);

            // Get the GUI name of the added points dataset and print to the console
            qDebug() << datasetGuiName << "was added";

            if (datasetGuiName.contains("Hsne scale"))
            {
                qDebug() << "Found HSNE scale";
                ViewPlugin* scatterplot = getRefineScatterplot();

                mv::gui::WidgetActions widgetActions = scatterplot->getChildren(true);

                scatterplot->printChildren();

                QString actionPath = "Settings/Datasets/Position";

                WidgetAction* action = scatterplot->findChildByPath(actionPath);

                DatasetPickerAction* datasetPickerAction = dynamic_cast<DatasetPickerAction*>(action);

                qDebug() << "Position action found";
                qDebug() << actionPath;
                qDebug() << action;
                qDebug() << datasetPickerAction;

                mv::Datasets datasets = datasetPickerAction->getDatasets();
                datasets.append(dataEvent->getDataset());

                for (mv::Dataset dataset : datasets)
                {
                    qDebug() << dataset->getGuiName();
                    qDebug() << dataset->getId();
                }

                datasetPickerAction->setDatasets(datasets);
                //datasetPickerAction->setCurrentDataset(dataEvent->getDataset());
                datasetPickerAction->setCurrentIndex(datasets.size()-1);
            }

            break;
        }

        default:
            break;
    }
}

ViewPlugin* RefinePlugin::getRefineScatterplot()
{
    std::vector<Plugin*> plugins = mv::plugins().getPluginsByType(mv::plugin::Type::VIEW);

    ViewPlugin* scatterplot = nullptr;
    for (Plugin* plugin : plugins)
    {
        qDebug() << plugin->getGuiName();
        if (plugin->getGuiName().contains("2"))
            scatterplot = dynamic_cast<ViewPlugin*>(plugin);
    }

    if (scatterplot == nullptr)
    {
        qDebug() << "Refine scatterplot found.";
    }

    return scatterplot;
}

void RefinePlugin::onRefine()
{
    {
        bool alreadyCreated = false;

        std::vector<Plugin*> plugins = mv::plugins().getPluginsByType(mv::plugin::Type::VIEW);

        ViewPlugin* scatterplot = nullptr;
        for (Plugin* plugin : plugins)
        {
            qDebug() << plugin->getGuiName();
            if (plugin->getGuiName().contains("Scatterplot"))
            {
                if (plugin->getGuiName().contains("2"))
                {
                    alreadyCreated = true;
                }
                mv::gui::WidgetActions widgetActions = plugin->getChildren(true);

                for (mv::gui::WidgetAction* action : widgetActions)
                {
                    qDebug() << action->getSerializationName();
                }

                scatterplot = dynamic_cast<ViewPlugin*>(plugin);
            }
        }

        if (scatterplot == nullptr)
        {
            qDebug() << "No scatterplot found.";
        }

        if (!alreadyCreated)
        {
            Plugin* plugin = mv::plugins().requestPlugin("Scatterplot View");
            ViewPlugin* viewPlugin = dynamic_cast<ViewPlugin*>(plugin);
            mv::workspaces().addViewPlugin(viewPlugin, scatterplot);
            qDebug() << plugin->getGuiName();
        }
    }
    {
        std::vector<Plugin*> plugins = mv::plugins().getPluginsByType(mv::plugin::Type::ANALYSIS);

        AnalysisPlugin* hsnePlugin = nullptr;
        for (Plugin* plugin : plugins)
        {
            qDebug() << plugin->getGuiName();
            if (plugin->getGuiName().contains("HSNE Analysis"))
            {
                mv::gui::WidgetActions widgetActions = plugin->getChildren(true);

                for (mv::gui::WidgetAction* action : widgetActions)
                {
                    qDebug() << action->getSerializationName();
                }

                hsnePlugin = dynamic_cast<AnalysisPlugin*>(plugin);
            }
        }
        if (hsnePlugin == nullptr)
        {
            qDebug() << "No HSNE plugin found.";
        }

        mv::Dataset<mv::DatasetImpl> pointData = hsnePlugin->getOutputDataset();

        mv::gui::WidgetActions widgetActions = pointData->getActions();
        qDebug() << "Meow";
        for (mv::gui::WidgetAction* action : widgetActions)
        {
            qDebug() << action->getSerializationName();
            if (action->getSerializationName().contains("HSNE Scale"))
            {
                for (mv::gui::WidgetAction* action2 : action->getChildren(true))
                {
                    qDebug() << "Action2: " << action2->getSerializationName();
                    if (action2->getSerializationName().contains("Refine selection"))
                    {
                        TriggerAction* refineAction = dynamic_cast<TriggerAction*>(action2);
                        refineAction->trigger();
                    }
                }
            }
        }
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
