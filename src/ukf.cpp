#include "ukf.h"
#include "tools.h"
#include "Eigen/Dense"
#include <iostream>

using namespace std;
using Eigen::MatrixXd;
using Eigen::VectorXd;
using std::vector;

/**
 * Initializes Unscented Kalman filter
 * This is scaffolding, do not modify
 */
UKF::UKF() {
  // if this is false, laser measurements will be ignored (except during init)
  use_laser_ = true;

  // if this is false, radar measurements will be ignored (except during init)
  use_radar_ = true;

  // initial state vector
  x_ = VectorXd(5);

  // initial covariance matrix
  P_ = MatrixXd(5, 5);

  // Process noise standard deviation longitudinal acceleration in m/s^2
  std_a_ = 1; // Change this

  // Process noise standard deviation yaw acceleration in rad/s^2
  std_yawdd_ = 1; // Change this
  
  //DO NOT MODIFY measurement noise values below these are provided by the sensor manufacturer.
  // Laser measurement noise standard deviation position1 in m
  std_laspx_ = 0.15;

  // Laser measurement noise standard deviation position2 in m
  std_laspy_ = 0.15;

  // Radar measurement noise standard deviation radius in m
  std_radr_ = 0.3;

  // Radar measurement noise standard deviation angle in rad
  std_radphi_ = 0.03;

  // Radar measurement noise standard deviation radius change in m/s
  std_radrd_ = 0.3;
  //DO NOT MODIFY measurement noise values above these are provided by the sensor manufacturer.

  is_initialized_ = false;
  n_x_ = 5;
  n_aug_ = 7;
  lambda_ = 3 - n_x_;
  weights_ = VectorXd(2*n_aug_+1);

  time_us_ = 0.0;

  Xsig_pred_ = MatrixXd(n_x_, 2*n_aug_+1);
  
  NIS_radar_ = 0.0;
  NIS_laser_ = 0.0;

}

UKF::~UKF(){}

/**
 * @param {MeasurementPackage} meas_package The latest measurement data of
 * either radar or laser.
 */
void UKF::ProcessMeasurement(MeasurementPackage measurement_pack) {
  /**
  TODO:

  Complete this function! Make sure you switch between lidar and radar
  measurements.
  */

	if (!is_initialized_) {
	    /**
	    TODO:
	      * Initialize the state ekf_.x_ with the first measurement.
	      * Create the covariance matrix.
	      * Remember: you'll need to convert radar from polar to cartesian coordinates.
	    */
	    // first measurement
	    //cout << "EKF: " << endl;
	    
	    x_ << 1,1,1,1,0.2;
	    P_ << 0.2, 0, 0, 0, 0,
	          0, 0.2, 0, 0, 0,
	          0, 0, 1, 0, 0,
	          0, 0, 0, 1, 0,
	          0, 0, 0, 0, 1;

	    if (measurement_pack.sensor_type_ == MeasurementPackage::RADAR && use_radar_) {
	      /**
	      Convert radar from polar to cartesian coordinates and initialize state.
	      */
	      float ro = measurement_pack.raw_measurements_(0);
	      float theta = measurement_pack.raw_measurements_(1);
	      float ro_dot = measurement_pack.raw_measurements_(2);
	   

	      // px & py coordinates
	      x_(0) = ro * cos(theta);
	      x_(1) = ro * sin(theta);

	      
	    }
	    else if (measurement_pack.sensor_type_ == MeasurementPackage::LASER && use_laser_) {
	      /**
	      Initialize state.
	      */
	      x_(0) = measurement_pack.raw_measurements_(0);
	      x_(1) = measurement_pack.raw_measurements_(1);
	    }

	    time_us_ = measurement_pack.timestamp_;

	    // done initializing, no need to predict or update
	    is_initialized_ = true;
	    return;
	}


	// Predict
	float delta_t = (measurement_pack.timestamp_ - time_us_)/1000000.0;
	time_us_ = measurement_pack.timestamp_;
	Prediction(delta_t);

	// Update
	if (measurement_pack.sensor_type_ == MeasurementPackage::LASER)
		UpdateLidar(measurement_pack);
	else if(measurement_pack.sensor_type_ == MeasurementPackage::RADAR)
		UpdateRadar(measurement_pack);
	
}
/**
 * Predicts sigma points, the state, and the state covariance matrix.
 * @param {double} delta_t the change in time (in seconds) between the last
 * measurement and this one.
 */
void UKF::Prediction(double delta_t) {
  /**
  TODO:

  Complete this function! Estimate the object's location. Modify the state
  vector, x_. Predict sigma points, the state, and the state covariance matrix.
  */

	MatrixXd Xsig = MatrixXd(n_x_, 2*n_x_ + 1);
	MatrixXd A = P_.llt().matrixL();

	lambda_ = 3 - n_x_;

	Xsig.col(0) = x_;

	for(int i = 0; i < n_x_; i++)
	{
		Xsig.col(i + 1) = x_ + sqrt(lambda_ + n_x_) * A.col(i);
		Xsig.col(i + 1 + n_x_) = x_ - sqrt(lambda_ + n_x_) * A.col(i);
	}


	// Create Augmented Sigma Points

	VectorXd x_aug = VectorXd(n_aug_);
	MatrixXd P_aug = MatrixXd(n_aug_,n_aug_);
	MatrixXd Xsig_aug = MatrixXd(n_aug_,2*n_aug_+1);
	lambda_ = 3 - n_aug_;

	// Create augmented mean state
	x_aug.head(5) = x_;
	x_aug(5) = 0;
	x_aug(6) = 0;


	// Create augmented covariance matrix
	P_aug.fill(0.0);
	P_aug.topLeftCorner(5,5) = P_;
	P_aug(5,5) = std_a_*std_a_;
	P_aug(6,6) = std_yawdd_*std_yawdd_;

	// create square root matrix
	MatrixXd L = P_aug.llt().matrixL();

	// Create augmented sigma points
	Xsig_aug.col(0) = x_aug;
	for(int i = 0; i < n_aug_; i++)
	{
		Xsig_aug.col(i+1) = x_aug + sqrt(lambda_ + n_aug_) * L.col(i);
		Xsig_aug.col(i+1+n_aug_) = x_aug - sqrt(lambda_ + n_aug_) * L.col(i);
	}





	// Predict Sigma Points


	for(int i = 0; i < 2*n_aug_+1; i++)
	{
		// extract values for better readability
		double p_x = Xsig_aug(0,i);
		double p_y = Xsig_aug(1,i);
		double v = Xsig_aug(2,i);
		double yaw = Xsig_aug(3,i);
		double yawd = Xsig_aug(4,i);
		double nu_a = Xsig_aug(5,i);
		double nu_yawdd = Xsig_aug(6,i);

		// predicted state values
		double px_p, py_p;

		// avoid division by zero
		if(fabs(yawd) > 0.01)
		{
			px_p = p_x + v/yawd * (sin(yaw + yawd*delta_t) - sin(yaw));
			py_p = p_y + v/yawd * (cos(yaw) - cos(yaw + yawd*delta_t));
		}
		else
		{
			px_p = p_x + v*delta_t*cos(yaw);
			py_p = p_y + v*delta_t*sin(yaw);
		}

		double v_p = v;
		double yaw_p = yaw + yawd*delta_t;
		double yawd_p = yawd;

		// add noise
		px_p = px_p + 0.5*nu_a*delta_t*delta_t * cos(yaw);
		py_p = py_p + 0.5*nu_a*delta_t*delta_t * sin(yaw);
		v_p = v_p + nu_a*delta_t;

		yaw_p = yaw_p + 0.5*nu_yawdd*delta_t*delta_t;
		yawd_p = yawd_p + nu_yawdd*delta_t;

		// write predicted sigma point into right column
		Xsig_pred_(0,i) = px_p;
		Xsig_pred_(1,i) = py_p;
		Xsig_pred_(2,i) = v_p;
		Xsig_pred_(3,i) = yaw_p;
		Xsig_pred_(4,i) = yawd_p;
	}

	// Predict mean and covariance
	double weight_0 = lambda_/(lambda_+n_aug_);
	weights_(0) = weight_0;
	for(int i = 1; i < 2*n_aug_+1; i++)
	{
		double weight = 0.5/(n_aug_+lambda_);
		weights_(i) = weight;
	}

	
	//Predict state mean
	x_.fill(0.0);
	for (int i = 0; i < 2 * n_aug_ + 1; i++)
		x_ = x_ + weights_(i) * Xsig_pred_.col(i);

	//predicted state covariance matrix
	P_.fill(0.0);
	for (int i = 0; i < 2 * n_aug_ + 1; i++) //iterate over sigma points
	{  
		// state difference
		VectorXd x_diff = Xsig_pred_.col(i) - x_;
		//angle normalization
		while (x_diff(3)> M_PI) x_diff(3) -= 2.*M_PI;
		while (x_diff(3)<-M_PI) x_diff(3) += 2.*M_PI;

		P_ = P_ + weights_(i) * x_diff * x_diff.transpose();
	}

}

/**
 * Updates the state and the state covariance matrix using a laser measurement.
 * @param {MeasurementPackage} meas_package
 */
void UKF::UpdateLidar(MeasurementPackage measurement_pack) {
  /**
  TODO:

  Complete this function! Use lidar data to update the belief about the object's
  position. Modify the state vector, x_, and covariance, P_.

  You'll also need to calculate the lidar NIS.
  */

	/***
	Need to:
		1. Extract values
		2. Transform sigma points into measurement space
		3. Calculate mean predicted measurement
		4. Calculate measurement covariance matrix
		5. Add a measurement noise covariance matrix
	*/


	// Lidar measures p_x and p_y
	int n_meas = 2;

	MatrixXd Zsig = MatrixXd(n_meas, 2*n_aug_ + 1);
	VectorXd z = measurement_pack.raw_measurements_;

	// Transform sigma points into measurement space
	for (int i = 0; i < 2 * n_aug_ + 1; i++) {  //2n+1 simga points

		// extract values for better readibility
		double p_x = Xsig_pred_(0, i);
		double p_y = Xsig_pred_(1, i);

		// measurement model
		Zsig(0, i) = p_x;
		Zsig(1, i) = p_y;
	}

	//Calculate mean predicted measurement
	VectorXd z_pred = VectorXd(n_meas);
	z_pred.fill(0.0);
	for (int i = 0; i < 2 * n_aug_ + 1; i++) 
		z_pred = z_pred + weights_(i) * Zsig.col(i);

	//Calculate measurement covariance matrix S
	MatrixXd S = MatrixXd(n_meas, n_meas);
	S.fill(0.0);
	for (int i = 0; i < 2 * n_aug_ + 1; i++) {  //2n+1 simga points

		//residual
		VectorXd z_diff = Zsig.col(i) - z_pred;

		S = S + weights_(i) * z_diff * z_diff.transpose();
	}

	// Add measurement noise covariance matrix
	MatrixXd R = MatrixXd(n_meas, n_meas);
	R << std_laspx_*std_laspx_, 0,
	   0, std_laspy_*std_laspy_;
	S = S + R;

	// Update for Lidar
	/* Need to:
		1. Calculate cross correlation matrix
		2. Calculate Kalman gain K
		3. Calculate residual
		4. Update state and mean covariance matrix
		5. Calculate NIS
	*/
	MatrixXd Tc = MatrixXd(n_x_, n_meas);
	Tc.fill(0.0);
	for (int i = 0; i < 2 * n_aug_ + 1; i++) {  //2n+1 simga points

		//residual
		VectorXd z_diff = Zsig.col(i) - z_pred;

		// state difference
		VectorXd x_diff = Xsig_pred_.col(i) - x_;

		Tc = Tc + weights_(i) * x_diff * z_diff.transpose();
	}
	//Kalman gain K;
	MatrixXd K = Tc * S.inverse();

	//residual
	VectorXd z_diff = z - z_pred;

	//update state mean and covariance matrix
	x_ = x_ + K * z_diff;
	P_ = P_ - K*S*K.transpose();

	// Calculate NIS
	NIS_laser_ = z_diff.transpose()*S.inverse()*z_diff;

}

/**
 * Updates the state and the state covariance matrix using a radar measurement.
 * @param {MeasurementPackage} meas_package
 */
void UKF::UpdateRadar(MeasurementPackage measurement_pack) {
	/**
	TODO:

	Complete this function! Use radar data to update the belief about the object's
	position. Modify the state vector, x_, and covariance, P_.

	You'll also need to calculate the radar NIS.
	*/

	// Lidar measures p_x and p_y
	int n_meas = 3;

	MatrixXd Zsig = MatrixXd(n_meas, 2*n_aug_ + 1);
	VectorXd z = measurement_pack.raw_measurements_;

	//transform sigma points into measurement space
 	for (int i = 0; i < 2 * n_aug_ + 1; i++) {  //2n+1 simga points

		// extract values for better readibility
		double p_x = Xsig_pred_(0, i);
		double p_y = Xsig_pred_(1, i);
		double v   = Xsig_pred_(2, i);
		double yaw = Xsig_pred_(3, i);

		double v1 = cos(yaw)*v;
		double v2 = sin(yaw)*v;

		// measurement model
		Zsig(0, i) = sqrt(p_x*p_x + p_y*p_y);                        //r
		Zsig(1, i) = atan2(p_y, p_x);                                 //phi
		Zsig(2, i) = (p_x*v1 + p_y*v2) / sqrt(p_x*p_x + p_y*p_y);   //r_dot
 	}

	//mean predicted measurement
	VectorXd z_pred = VectorXd(n_meas);
	z_pred.fill(0.0);
	for (int i = 0; i < 2 * n_aug_ + 1; i++)
		z_pred = z_pred + weights_(i) * Zsig.col(i);

	//measurement covariance matrix S
	MatrixXd S = MatrixXd(n_meas, n_meas);
	S.fill(0.0);
	for (int i = 0; i < 2 * n_aug_ + 1; i++) {  //2n+1 simga points
	    //residual
	    VectorXd z_diff = Zsig.col(i) - z_pred;

    //angle normalization
    while (z_diff(1)> M_PI) z_diff(1) -= 2.*M_PI;
    while (z_diff(1)<-M_PI) z_diff(1) += 2.*M_PI;

    S = S + weights_(i) * z_diff * z_diff.transpose();
  }

   //add measurement noise covariance matrix
  MatrixXd R = MatrixXd(n_meas, n_meas);
  R << std_radr_*std_radr_, 0, 0,
       0, std_radphi_*std_radphi_, 0,
       0, 0, std_radrd_*std_radrd_;
  S = S + R;


  // Update Radar


  //calculate cross correlation matrix
  MatrixXd Tc = MatrixXd(n_x_, n_meas);
  Tc.fill(0.0);
  for (int i = 0; i < 2 * n_aug_ + 1; i++) {  //2n+1 simga points

    //residual
    VectorXd z_diff = Zsig.col(i) - z_pred;
    //angle normalization
    while (z_diff(1)> M_PI) z_diff(1) -= 2.*M_PI;
    while (z_diff(1)<-M_PI) z_diff(1) += 2.*M_PI;

    // state difference
    VectorXd x_diff = Xsig_pred_.col(i) - x_;
    //angle normalization
    while (x_diff(3)> M_PI) x_diff(3) -= 2.*M_PI;
    while (x_diff(3)<-M_PI) x_diff(3) += 2.*M_PI;

    Tc = Tc + weights_(i) * x_diff * z_diff.transpose();
  }

  //Kalman gain K;
  MatrixXd K = Tc * S.inverse();

  //residual
  VectorXd z_diff = z - z_pred;

  //angle normalization
  while (z_diff(1)> M_PI) z_diff(1) -= 2.*M_PI;
  while (z_diff(1)<-M_PI) z_diff(1) += 2.*M_PI;

  //update state mean and covariance matrix
  x_ = x_ + K * z_diff;
  P_ = P_ - K*S*K.transpose();

  // Calculate NIS
  NIS_radar_ = z_diff.transpose()*S.inverse()*z_diff;
}
