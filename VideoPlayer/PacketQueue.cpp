#include "stdafx.h"

#include "PacketQueue.h"

//void packet_queue_init(PacketQueue *q)
//{
//	memset(q, 0, sizeof(PacketQueue));
//	q->mutex = SDL_CreateMutex();
//}
//
//
//int packet_queue_put(PacketQueue *q, AVPacket *pkt)
//{
//
//	AVPacketList *pkt1;
//	if (av_dup_packet(pkt) < 0)
//	{
//		return -1;
//	}
//
//
//	pkt1 = (AVPacketList*)av_malloc(sizeof(AVPacketList));
//	if (!pkt1)
//		return -1;
//	pkt1->pkt = *pkt;
//	pkt1->next = NULL;
//
//	SDL_LockMutex(q->mutex);
//
//	if (!q->last_pkt)
//		q->first_pkt = pkt1;
//	else
//		q->last_pkt->next = pkt1;
//	q->last_pkt = pkt1;
//	q->nb_packets++;
//	q->size += pkt1->pkt.size;
//
//	SDL_UnlockMutex(q->mutex);
//	return 0;
//}
//
//

//
//
//int packet_queue_get(PacketQueue *q, AVPacket *pkt)
//{
//	AVPacketList *packetlist;
//	int ret;
//	SDL_LockMutex(q->mutex);
//	packetlist = q->first_pkt;
//	if (packetlist)
//	{
//		q->first_pkt = packetlist->next;
//		if (!q->first_pkt)
//			q->last_pkt = NULL;
//		q->nb_packets--;
//		q->size -= packetlist->pkt.size;
//		*pkt = packetlist->pkt;
//		av_free(packetlist);
//		ret = TRUE;
//	}
//	else
//	{
//		ret = FALSE;
//	}
//	SDL_UnlockMutex(q->mutex);
//	return ret;
//}
//
//
//void packet_queue_exit(PacketQueue *q)
//{
//	packet_queue_clear(q);
//
//	SDL_DestroyMutex(q->mutex);
//}


void packet_queue_init(PacketQueue *q)
{
	memset(q, 0, sizeof(PacketQueue));
	q->mutex = SDL_CreateMutex();
	q->cond = SDL_CreateCond();
}


int packet_queue_put(PacketQueue *q, AVPacket *pkt)
{
	AVPacketDeque *pkt1;
	if (av_dup_packet(pkt) < 0)
		return FALSE;
	pkt1 = (AVPacketDeque *)av_malloc(sizeof(AVPacketDeque));

	if (!pkt1)
		return FALSE;

	pkt1->pkt = *pkt;

	SDL_LockMutex(q->mutex);

	if (q->last_pkt == NULL)
	{
		q->first_pkt = pkt1;
		q->last_pkt = pkt1;
		pkt1->prev = NULL;
		pkt1->next = NULL;
	}
	else if (pkt1->pkt.pts >= q->last_pkt->pkt.pts)
	{
		pkt1->prev = q->last_pkt;
		q->last_pkt->next = pkt1;
		q->last_pkt = pkt1;
		pkt1->next = NULL;
	}
	else
	{
		AVPacketDeque* insertr = q->last_pkt;
		while (insertr->prev != NULL && pkt1->pkt.pts < insertr->pkt.pts)
		{
			insertr = insertr->prev;
		}
		AVPacketDeque* insertl = insertr->prev;

		if (insertl == NULL)
		{
			insertr->prev = pkt1;
			pkt1->next = insertr;
			pkt1->prev = NULL;
			q->first_pkt = pkt1;
		}
		else
		{
			insertl->next = pkt1;
			pkt1->prev = insertl;
			insertr->prev = pkt1;
			pkt1->next = insertr;
		}
	}

	q->nb_packets++;
	q->size += pkt1->pkt.size;
	SDL_CondSignal(q->cond);
	SDL_UnlockMutex(q->mutex);
	return TRUE;
}

int packet_queue_get(PacketQueue *q, AVPacket *pkt/*, int block*/)
{
	AVPacketDeque *pkt1;
	int ret;
	SDL_LockMutex(q->mutex);

	pkt1 = q->first_pkt;
	if (pkt1)
	{
		q->first_pkt = pkt1->next;

		if (!q->first_pkt)
		{
			q->last_pkt = NULL;
		}
		else
		{
			q->first_pkt->prev = NULL;
		}

		q->nb_packets--;
		q->size -= pkt1->pkt.size;

		*pkt = pkt1->pkt;
		av_free(pkt1);
		ret = 1;
	}
	else
	{
		ret = -1;
	}
	SDL_UnlockMutex(q->mutex);
	return ret;
}

void packet_queue_clear(PacketQueue *q)
{
	SDL_LockMutex(q->mutex);

	AVPacketDeque *tmp;
	while (1)
	{
		tmp = q->first_pkt;
		if (tmp == NULL) break;
		q->first_pkt = tmp->next;
		q->size -= tmp->pkt.size;
		av_free(tmp);
	}
	q->last_pkt = NULL;
	q->nb_packets = 0;
	q->size = 0;

	SDL_UnlockMutex(q->mutex);
}