#include <QGridLayout>
#include <QErrorMessage>
#include "mainwindow.h"

#include <vtkRenderer.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkInteractorStyleTrackballCamera.h>
#include <vtkRendererCollection.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkGenericDataObjectReader.h>

#include <vtkCamera.h>
#include <vtkImageMapToWindowLevelColors.h>
#include <vtkImageData.h>
#include <vtkLookupTable.h>
#include <itkImage.h>
#include <itkExceptionObject.h>

#include <vtkVertexGlyphFilter.h>
#include "generatedsurface.h"

#include <vtkDataObject.h>
#include <vtkDataSetMapper.h>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
	reader = vtkKWImageIO::New();
	imageviewer = vtkImageViewer2::New();
	localVTKImage = vtkKWImage::New();
	renderPreview = new QVTKWidget();
	widgetCortex = new QVTKWidget();
	horizontalSlider = new QSlider();
	ui = new Ui::MainWindow;
	ui->setupUi(this);

	// add VTK widgets
	//ui->verticalLayoutL->addWidget(widget);
	ui->verticalLayoutR->addWidget(widgetCortex);
	//QGridLayout *layout = new QGridLayout();
	horizontalSlider->setEnabled(false);
	horizontalSlider->setRange(0, 0);
	horizontalSlider->setOrientation(Qt::Horizontal);
	ui->verticalLayoutL->addWidget(renderPreview, 0, 0);
	ui->verticalLayoutL->addWidget(horizontalSlider, 1, 0);
	//widgetCortex->setPalette(QPalette(QColor(Qt::green)));
	//renderPreview->setPalette(QPalette(QColor(Qt::green)));

	// Ugly hack to remove the garbage in the widget before that the
	// image is opened. Create an empty image and set it as imput data.
	// Without the image, it will cause an error.
	vtkImageData *emptyImage = vtkImageData::New();
	emptyImage->SetExtent(0, 0, 0, 0, 0, 0);
#if (VTK_MAJOR_VERSION < 6)
	emptyImage->SetScalarType(VTK_INT);
	emptyImage->SetNumberOfScalarComponents(1);
	emptyImage->AllocateScalars();
	m_imageviewer->GetWindowLevel()->SetInput(emptyImage);
#else
	emptyImage->AllocateScalars(VTK_INT, 1);
	imageviewer->GetWindowLevel()->SetInputData(emptyImage);
#endif
	emptyImage->Delete();

	renderPreview->SetRenderWindow(imageviewer->GetRenderWindow());
	imageviewer->SetSliceOrientationToXY();
	imageviewer->SetSlice(0);
	imageviewer->GetRenderer()->SetBackground(1, 1, 1);
	imageviewer->GetRenderer()->GetActiveCamera()->ParallelProjectionOn();
	imageviewer->GetRenderWindow()->GetInteractor()->SetInteractorStyle(0);
	renderPreview->update();

	// set up interactor
	interactor = vtkSmartPointer<vtkRenderWindowInteractor>::New();
	interactor->SetRenderWindow(widgetCortex->GetRenderWindow());

	// allow the user to interactively manipulate (rotate, pan, etc.) the camera, the viewpoint of the scene.
	auto style = vtkSmartPointer<vtkInteractorStyleTrackballCamera>::New();
	interactor->SetInteractorStyle(style);

	// set default data set
	filename = "../../data/nucleon.mhd";
	connect(horizontalSlider, SIGNAL(valueChanged(int)), this, SLOT(onSliderChange(int)));
	connect(ui->action_Open, SIGNAL(triggered()), this, SLOT(loadFile()));
}

MainWindow::~MainWindow()
{
	delete ui;
	delete renderPreview;
	delete widgetCortex;
	delete horizontalSlider;
	localVTKImage->Delete();
	reader->Delete();
	imageviewer->Delete();
}

void MainWindow::createActions()
{
}

bool MainWindow::onSliderChange(int z)
{
	imageviewer->SetSlice(z);
	renderPreview->update();
	return true;
}

bool MainWindow::on_action_ContourSurface_triggered()
{
	GeneratedSurface *generatedSurface = GeneratedSurface::New();

	double imageRange[2];
	localVTKImage->GetVTKImage()->GetScalarRange(imageRange);
	double contourValue = imageRange[0] + (imageRange[1] - imageRange[0]) / 5.0; //the best seems to be 20% of max, tested on Colin27
	generatedSurface->SetImageObject(localVTKImage->GetVTKImage());
	generatedSurface->SetContourValue(contourValue);
	generatedSurface->SetGaussianSmoothingFlag(true);
	vtkPolyData *surface = generatedSurface->GenerateSurface();
	//vtkSmartPointer<vtkPolyDataReader> reader =
	//	vtkSmartPointer<vtkPolyDataReader>::New();
	//reader->SetInputDataObject(surface);
	//reader->Update();

	// Visualize
	vtkSmartPointer<vtkPolyDataMapper> mapper =
		vtkSmartPointer<vtkPolyDataMapper>::New();
	//mapper->SetInputConnection(reader->GetOutputPort());
	mapper->SetInputData(surface);
	mapper->ScalarVisibilityOff();

	vtkSmartPointer<vtkActor> actor =
		vtkSmartPointer<vtkActor>::New();
	actor->SetMapper(mapper);
	actor->GetProperty()->SetDiffuseColor(1, 1, 1);

	vtkSmartPointer<vtkRenderer> renderer =
		vtkSmartPointer<vtkRenderer>::New();
	vtkSmartPointer<vtkRenderWindow> renderWindow =
		vtkSmartPointer<vtkRenderWindow>::New();
	renderWindow->AddRenderer(renderer);
	vtkSmartPointer<vtkRenderWindowInteractor> renderWindowInteractor =
		vtkSmartPointer<vtkRenderWindowInteractor>::New();
	renderWindowInteractor->SetRenderWindow(renderWindow);

	renderer->AddActor(actor);
	renderer->SetBackground(.2, .3, .4);

	// clean previous renderers and then add the current renderer
	auto window = widgetCortex->GetRenderWindow();
	auto collection = window->GetRenderers();
	auto item = collection->GetNumberOfItems();
	while (item)
	{
		window->RemoveRenderer(collection->GetFirstRenderer());
		item = collection->GetNumberOfItems();
	}
	window->AddRenderer(renderer);
	window->Render();

	// initialize the interactor
	interactor->Initialize();
	interactor->Start();
	return true;
}

void MainWindow::loadFile()
{
	// show file dialog. change filename only when the new filename is not empty.
	QString filter("Available Filetypes (*.nii.gz *.nii *.vtk)\n");
	QString filename_backup = filename;
	filename_backup = QFileDialog::getOpenFileName(this, QString(tr("Open a file")), filename_backup, filter);

	if (!filename_backup.trimmed().isEmpty())
	{
		filename = filename_backup;
		// show filename on window title
		this->setWindowTitle(filename);

		if (filename.endsWith("nii") || filename.endsWith("nii.gz"))
		{
			reader->SetFileName(filename.toStdString());
			try 
			{
				reader->ReadImage();
			}
			catch (itk::ExceptionObject excp) 
			{
				std::cerr << "Error while opening image" << excp.GetDescription() << std::endl;

				QErrorMessage error_message;
				error_message.showMessage(excp.GetDescription());
				error_message.exec();
				return;
			}

			double range[2];
			localVTKImage = reader->HarvestReadImage();
			localVTKImage->GetVTKImage()->GetScalarRange(range);
			vtkLookupTable *lookupTable = vtkLookupTable::New();
			lookupTable->SetValueRange(0.0, 1.0);
			lookupTable->SetSaturationRange(0.0, 0.0);
			lookupTable->SetRampToLinear();
			lookupTable->SetRange(range);
			lookupTable->Build();
			imageviewer->GetWindowLevel()->SetLookupTable(lookupTable);
			lookupTable->Delete();

#if (VTK_MAJOR_VERSION < 6)
			m_imageviewer->GetWindowLevel()->SetInput(m_localVTKImage->GetVTKImage());
#else
			imageviewer->GetWindowLevel()->SetInputData(localVTKImage->GetVTKImage());
#endif

			int *dimensions = localVTKImage->GetVTKImage()->GetDimensions();
			//imageviewer->GetRenderWindow()->SetSize(200, 200);
			imageviewer->Render();
			imageviewer->GetRenderer()->ResetCamera();
			imageviewer->GetRenderer()->GetActiveCamera()->SetParallelScale(dimensions[1]);
			renderPreview->update();

			horizontalSlider->setValue(0);
			horizontalSlider->setEnabled(true);
			horizontalSlider->setRange(0, dimensions[2]);

			std::ostringstream str_dimensions;
			str_dimensions << "[" << dimensions[0] << "," << dimensions[1] << "," << dimensions[2] << "]";
		}
		else if (filename.endsWith("vtk"))
		{
			// get local 8-bit representation of the string in locale encoding (in case the filename contains non-ASCII characters) 
			QByteArray ba = filename.toLocal8Bit();
			const char *filename_str = ba.data();

			// Read the file
			vtkSmartPointer<vtkGenericDataObjectReader> reader =
				vtkSmartPointer<vtkGenericDataObjectReader>::New();
			reader->SetFileName(filename_str);
			reader->Update();

			// Visualize
			vtkSmartPointer<vtkPolyDataMapper> mapper =
				vtkSmartPointer<vtkPolyDataMapper>::New();
			mapper->SetInputConnection(reader->GetOutputPort());
			mapper->ScalarVisibilityOff();

			vtkSmartPointer<vtkActor> actor =
				vtkSmartPointer<vtkActor>::New();
			actor->SetMapper(mapper);
			actor->GetProperty()->SetDiffuseColor(1, 1, 1);

			vtkSmartPointer<vtkRenderer> renderer =
				vtkSmartPointer<vtkRenderer>::New();
			renderer->AddActor(actor);
			renderer->SetBackground(.2, .3, .4);

			// clean previous renderers and then add the current renderer
			auto window = widgetCortex->GetRenderWindow();
			auto collection = window->GetRenderers();
			auto item = collection->GetNumberOfItems();
			while (item)
			{
				window->RemoveRenderer(collection->GetFirstRenderer());
				item = collection->GetNumberOfItems();
			}
			window->AddRenderer(renderer);
			window->Render();

			// initialize the interactor
			interactor->Initialize();
			interactor->Start();
		}
	}
	else
	{
		return;
	}
}

