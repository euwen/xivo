#include "pybind11/eigen.h"
#include "pybind11/numpy.h"
#include "pybind11/pybind11.h"

#include "estimator.h"
#include "opencv2/core/eigen.hpp"
#include "opencv2/highgui/highgui.hpp"
#include "utils.h"

// for visualization
#include "viewer.h"
#include "visualize.h"

typedef Eigen::Stride<Eigen::Dynamic, Eigen::Dynamic> Strides;
typedef Eigen::Matrix<int, Eigen::Dynamic, Eigen::Dynamic> Matrix;
typedef Matrix::Scalar Scalar;
constexpr bool rowMajor = Matrix::Flags & Eigen::RowMajorBit;
  
namespace py = pybind11;
using namespace xivo;

class EstimatorWrapper {
public:
  EstimatorWrapper(const std::string &cfg_path,
                   const std::string &viewer_cfg_path,
                   const std::string &name)
    : name_{name}, imu_calls_{0}, visual_calls_{0} {

    if (!glog_init_) {
      google::InitGoogleLogging("pyxivo");
      glog_init_ = true;
    }

    auto cfg = LoadJson(cfg_path);
    // estimator_ = std::unique_ptr<Estimator>(new Estimator{cfg});
    estimator_ = CreateSystem(cfg);

    if (!viewer_cfg_path.empty()) {
      auto viewer_cfg = LoadJson(viewer_cfg_path);
      viewer_ = std::unique_ptr<Viewer>(new Viewer{viewer_cfg, name});
    }
  }

  void InertialMeas(uint64_t ts, double wx, double wy, double wz, double ax,
                    double ay, double az) {

    // std::cout << "InertialMeas called on " << name_ << " " 
    //   << ++imu_calls_ << " times" << std::endl;

    estimator_->InertialMeas(timestamp_t{ts}, {wx, wy, wz}, {ax, ay, az});

    if (viewer_) {
      viewer_->Update_gsb(estimator_->gsb());
      viewer_->Update_gsc(estimator_->gsc());
    }
  }

  void VisualMeas(uint64_t ts, std::string &image_path) {

    // std::cout << "VisualMeas called on " << name_ << " " 
    //   << ++visual_calls_ << " times" << std::endl;

    auto image = cv::imread(image_path);

    estimator_->VisualMeas(timestamp_t{ts}, image);

    if (viewer_) {
      auto disp = Canvas::instance()->display();

      if (!disp.empty()) {
        LOG(INFO) << "Display image is ready";
        // cv::imshow("tracker", disp);
        // if (cv::waitKey(cfg.get("wait_time", 5).asInt()) == 'q') break;
        viewer_->Update(disp);
      }
    }
  }


  void VisualMeas2(uint64_t ts, py::array_t<int, py::array::c_style | py::array::forcecast> b)
  {
    py::buffer_info info = b.request();

    cv::Mat image(640, 480, CV_32S, info.ptr);

    estimator_->VisualMeas(timestamp_t{ts}, image);

    if (viewer_) {
      auto disp = Canvas::instance()->display();
      if (!disp.empty()) {
        LOG(INFO) << "Display image is ready";
        viewer_->Update(disp);
      }
    }
  }


  Eigen::Matrix<double, 3, 4> gsb() { return estimator_->gsb().matrix3x4(); }
  Eigen::Matrix<double, 3, 4> gsc() { return estimator_->gsc().matrix3x4(); }
  Eigen::Matrix<double, 3, 4> gbc() { return estimator_->gbc().matrix3x4(); }
  Eigen::Matrix<double, -1, -1> Pstate() { return estimator_->Pstate(); }
  Vec3 Vsb() { return estimator_->Vsb(); }
  Vec3 bg() { return estimator_->bg(); }
  Vec3 ba() { return estimator_->ba(); }
  Mat3 Rg() { return estimator_->Rg().matrix(); }
  number_t td() { return estimator_->td(); }
  Mat3 Ca() { return estimator_->Ca(); }
  Mat3 Cg() { return estimator_->Cg(); }

  bool MeasurementUpdateInitialized() { 
    return estimator_->MeasurementUpdateInitialized();
  }
  Vec3 inn_Wsb() { return estimator_->inn_Wsb(); }
  Vec3 inn_Tsb() { return estimator_->inn_Tsb(); }
  Vec3 inn_Vsb() { return estimator_->inn_Vsb(); }

  uint64_t now() const { return estimator_->ts().count(); }

  int gauge_group() { return estimator_->gauge_group(); }

  MatX InstateFeaturePositions(int n_output) {
    return estimator_->InstateFeaturePositions(n_output);
  }

  MatX InstateFeatureCovs(int n_output) {
    return estimator_->InstateFeatureCovs(n_output);
  }

  MatXi InstateFeatureIDs(int n_output) {
    return estimator_->InstateFeatureIDs(n_output);
  }

  int num_instate_features() { return estimator_->num_instate_features(); }

  void Visualize() {
    if (viewer_)
      viewer_->Refresh();
  }

private:
  // std::unique_ptr<Estimator> estimator_;
  EstimatorPtr estimator_;
  std::unique_ptr<Viewer> viewer_;
  static bool glog_init_;
  std::string name_;
  int imu_calls_, visual_calls_;
};

bool EstimatorWrapper::glog_init_{false};

PYBIND11_MODULE(pyxivo, m) {
  m.doc() = "python binding of XIVO (Xiaohan's Inertial-aided Visual Odometry)";
  py::class_<EstimatorWrapper>(m, "Estimator")
      .def(py::init<const std::string &, const std::string &,
                    const std::string &>())
      .def("InertialMeas", &EstimatorWrapper::InertialMeas)
      .def("VisualMeas", &EstimatorWrapper::VisualMeas)
      .def("VisualMeas2", &EstimatorWrapper::VisualMeas2)
      .def("gbc", &EstimatorWrapper::gbc)
      .def("gsb", &EstimatorWrapper::gsb)
      .def("gsc", &EstimatorWrapper::gsc)
      .def("Vsb", &EstimatorWrapper::Vsb)
      .def("inn_Tsb", &EstimatorWrapper::inn_Tsb)
      .def("inn_Wsb", &EstimatorWrapper::inn_Wsb)
      .def("inn_Vsb", &EstimatorWrapper::inn_Vsb)
      .def("Pstate", &EstimatorWrapper::Pstate)
      .def("bg", &EstimatorWrapper::bg)
      .def("ba", &EstimatorWrapper::ba)
      .def("Rg", &EstimatorWrapper::Rg)
      .def("td", &EstimatorWrapper::td)
      .def("Ca", &EstimatorWrapper::Ca)
      .def("Cg", &EstimatorWrapper::Cg)
      .def("InstateFeaturePositions", &EstimatorWrapper::InstateFeaturePositions)
      .def("InstateFeatureCovs", &EstimatorWrapper::InstateFeatureCovs)
      .def("InstateFeatureIDs", &EstimatorWrapper::InstateFeatureIDs)
      .def("num_instate_features", &EstimatorWrapper::num_instate_features)
      .def("now", &EstimatorWrapper::now)
      .def("Visualize", &EstimatorWrapper::Visualize)
      .def("gauge_group", &EstimatorWrapper::gauge_group)
      .def("MeasurementUpdateInitialized", &EstimatorWrapper::MeasurementUpdateInitialized);
}
