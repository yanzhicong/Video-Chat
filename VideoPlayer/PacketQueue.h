
#pragma once

#include "stdafx.h"


extern"C"
{
#include <libavformat\avformat.h>
#include <libavcodec\avcodec.h>
#include <libswresample\swresample.h>
#include <libavutil\opt.h>
#include <libswscale\swscale.h>
}


#include <SDL.h>

//typedef struct PacketQueue
//{
//	AVPacketList *first_pkt, *last_pkt;
//	int nb_packets;
//	int size;
//	SDL_mutex *mutex;
//	SDL_cond *cond;
//} PacketQueue;
//
//
//void packet_queue_init(PacketQueue *q);
//
//
//int packet_queue_put(PacketQueue *q, AVPacket *pkt);
//
//
//void packet_queue_clear(PacketQueue *q);
//
//
//int packet_queue_get(PacketQueue *q, AVPacket *pkt);
//
//
//void packet_queue_exit(PacketQueue *q);


typedef struct AVPacketDeque
{
	AVPacket pkt;
	AVPacketDeque *next;
	AVPacketDeque *prev;
}AVPacketDeque;

typedef struct PacketQueue
{
	AVPacketDeque *first_pkt, *last_pkt;
	int nb_packets;
	int size;
	SDL_mutex *mutex;
	SDL_cond *cond;
} PacketQueue;/*
PacketQueue audioq;
int quit = 0;*/

void packet_queue_init(PacketQueue *q);

int packet_queue_put(PacketQueue *q, AVPacket *pkt);

int packet_queue_get(PacketQueue *q, AVPacket *pkt/*, int block*/);

void packet_queue_clear(PacketQueue *q);