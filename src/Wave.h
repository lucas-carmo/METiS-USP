#pragma once

#include <string>
#include <math.h>


class Wave{
private:
    double m_height;
    double m_period;
    double m_direction;

public:
	Wave(double height = 0, double period = 0, double direction =0)
		: m_height(height), m_period(period), m_direction(direction)
	{}

	/*****************************************************
		Setters
	*****************************************************/
    void readRegWave(const std::string &wholeWaveLine);

	/*****************************************************
		Getters
	*****************************************************/
	double getHeight() const;
	double getPeriod() const;
	double getDirection() const;

	/*****************************************************
		Wave properties derived from the others
	*****************************************************/		
	// Essas nao precisam da profundidade
	// double freq();
	// double angFreq();

	// Essas precisam. Pensar em um jeito de retornar p/ prof. infinita quando nao 
	// especificar a profundidade (mas cuidado com isso, pra nao usar a versao errada)
	// double waveNumber();
	// double waveLength();
};
